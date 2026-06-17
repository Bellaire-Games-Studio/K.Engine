#pragma once
// -----------------------------------------------------------------------------
// K.Engine ECS
//
//  A small, dependency-free entity-component-system built on sparse sets, in the
//  spirit of EnTT but trimmed to what the engine needs. Components are stored in
//  packed (dense) arrays for cache-friendly iteration; entities are 32-bit
//  handles carrying an index + version so stale handles are detected after a
//  slot is recycled.
//
//  Usage:
//      KDot::ecs::Registry reg;
//      auto e = reg.Create();
//      reg.Emplace<Transform>(e, position);
//      reg.View<Transform, Rigidbody>([](auto entity, Transform& t, Rigidbody& rb){ ... });
//      reg.Destroy(e);
//
//  Note: do not Create/Destroy entities or add/remove the iterated component
//  type *during* a View() over that type (swap-remove invalidates iteration).
// -----------------------------------------------------------------------------
#include <cstdint>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace KDot
{
    namespace ecs
    {
        using Entity        = std::uint32_t;
        using EntityIndex   = std::uint32_t;
        using EntityVersion = std::uint32_t;

        constexpr std::uint32_t kIndexBits   = 20;                       // up to ~1M live entities
        constexpr std::uint32_t kIndexMask   = (1u << kIndexBits) - 1u;
        constexpr std::uint32_t kVersionMask = (1u << (32 - kIndexBits)) - 1u;
        constexpr Entity        kNull        = ~Entity(0);

        constexpr EntityIndex   IndexOf(Entity e)   { return e & kIndexMask; }
        constexpr EntityVersion VersionOf(Entity e) { return (e >> kIndexBits) & kVersionMask; }
        constexpr Entity        Make(EntityIndex i, EntityVersion v)
        {
            return (i & kIndexMask) | ((v & kVersionMask) << kIndexBits);
        }

        // ---------------------------------------------------------------------
        // Component storage
        // ---------------------------------------------------------------------
        struct IPool
        {
            virtual ~IPool() = default;
            virtual bool Remove(EntityIndex idx) = 0;
            virtual bool Has(EntityIndex idx) const = 0;
        };

        template <typename T>
        class Pool final : public IPool
        {
        public:
            bool Has(EntityIndex idx) const override
            {
                return idx < m_Sparse.size() && m_Sparse[idx] != kTomb;
            }

            T& Get(EntityIndex idx) { return m_Dense[m_Sparse[idx]]; }
            const T& Get(EntityIndex idx) const { return m_Dense[m_Sparse[idx]]; }

            template <typename... Args>
            T& Emplace(EntityIndex idx, Args&&... args)
            {
                if (idx >= m_Sparse.size())
                    m_Sparse.resize(idx + 1, kTomb);

                if (m_Sparse[idx] != kTomb) // replace existing component
                {
                    m_Dense[m_Sparse[idx]] = T{std::forward<Args>(args)...};
                    return m_Dense[m_Sparse[idx]];
                }

                m_Sparse[idx] = static_cast<std::uint32_t>(m_Dense.size());
                m_Owners.push_back(idx);
                m_Dense.emplace_back(std::forward<Args>(args)...);
                return m_Dense.back();
            }

            bool Remove(EntityIndex idx) override
            {
                if (!Has(idx))
                    return false;

                const std::uint32_t slot = m_Sparse[idx];
                const std::uint32_t last = static_cast<std::uint32_t>(m_Dense.size()) - 1;

                m_Dense[slot]  = std::move(m_Dense[last]);   // swap-remove keeps the array packed
                m_Owners[slot] = m_Owners[last];
                m_Sparse[m_Owners[slot]] = slot;

                m_Dense.pop_back();
                m_Owners.pop_back();
                m_Sparse[idx] = kTomb;
                return true;
            }

            std::vector<T>&           Data()    { return m_Dense; }
            std::vector<EntityIndex>& Owners()  { return m_Owners; }
            std::size_t               Size() const { return m_Dense.size(); }

        private:
            static constexpr std::uint32_t kTomb = ~std::uint32_t(0);
            std::vector<std::uint32_t> m_Sparse; // entity index -> dense slot
            std::vector<EntityIndex>   m_Owners; // dense slot   -> entity index
            std::vector<T>             m_Dense;  // packed components
        };

        // ---------------------------------------------------------------------
        // Registry
        // ---------------------------------------------------------------------
        class Registry
        {
        public:
            Entity Create()
            {
                if (!m_Free.empty())
                {
                    const EntityIndex idx = m_Free.back();
                    m_Free.pop_back();
                    return Make(idx, m_Versions[idx]);
                }
                const EntityIndex idx = static_cast<EntityIndex>(m_Versions.size());
                m_Versions.push_back(0);
                return Make(idx, 0);
            }

            // A handle is valid only while its version still matches the slot's
            // version. Destroy() bumps the slot version, so a freed slot can
            // never match an outstanding handle -> no separate "alive" bitset.
            bool Valid(Entity e) const
            {
                const EntityIndex idx = IndexOf(e);
                return idx < m_Versions.size()
                    && (m_Versions[idx] & kVersionMask) == VersionOf(e);
            }

            void Destroy(Entity e)
            {
                if (!Valid(e))
                    return;
                const EntityIndex idx = IndexOf(e);
                for (auto& kv : m_Pools)
                    kv.second->Remove(idx);
                m_Versions[idx] = (m_Versions[idx] + 1) & kVersionMask; // invalidate stale handles
                m_Free.push_back(idx);
            }

            template <typename T, typename... Args>
            T& Emplace(Entity e, Args&&... args)
            {
                return Assure<T>().Emplace(IndexOf(e), std::forward<Args>(args)...);
            }

            template <typename T>
            bool Has(Entity e) const
            {
                const Pool<T>* p = GetPool<T>();
                return p && p->Has(IndexOf(e));
            }

            template <typename T>
            T& Get(Entity e) { return Assure<T>().Get(IndexOf(e)); }

            template <typename T>
            T* TryGet(Entity e)
            {
                Pool<T>* p = GetPool<T>();
                return (p && p->Has(IndexOf(e))) ? &p->Get(IndexOf(e)) : nullptr;
            }

            template <typename T>
            void Remove(Entity e)
            {
                if (Pool<T>* p = GetPool<T>())
                    p->Remove(IndexOf(e));
            }

            // Iterate every entity owning all of <T, Others...>.
            // fn signature: void(Entity, T&, Others&...)
            template <typename T, typename... Others, typename Fn>
            void View(Fn&& fn)
            {
                Pool<T>* base = GetPool<T>();
                if (!base)
                    return;

                std::vector<EntityIndex>& owners = base->Owners();
                for (std::size_t i = 0; i < owners.size(); ++i)
                {
                    const EntityIndex idx = owners[i];
                    if ((HasIdx<Others>(idx) && ...))
                        fn(Make(idx, m_Versions[idx]), base->Get(idx), GetPool<Others>()->Get(idx)...);
                }
            }

            std::size_t AliveCount() const { return m_Versions.size() - m_Free.size(); }

        private:
            template <typename T>
            Pool<T>& Assure()
            {
                const std::type_index key(typeid(T));
                auto it = m_Pools.find(key);
                if (it == m_Pools.end())
                {
                    auto pool = std::make_unique<Pool<T>>();
                    Pool<T>& ref = *pool;
                    m_Pools.emplace(key, std::move(pool));
                    return ref;
                }
                return *static_cast<Pool<T>*>(it->second.get());
            }

            template <typename T>
            Pool<T>* GetPool() const
            {
                auto it = m_Pools.find(std::type_index(typeid(T)));
                return it == m_Pools.end() ? nullptr : static_cast<Pool<T>*>(it->second.get());
            }

            template <typename T>
            bool HasIdx(EntityIndex idx) const
            {
                const Pool<T>* p = GetPool<T>();
                return p && p->Has(idx);
            }

            std::vector<EntityVersion> m_Versions;
            std::vector<EntityIndex>   m_Free;
            std::unordered_map<std::type_index, std::unique_ptr<IPool>> m_Pools;
        };
    } // namespace ecs
} // namespace KDot
