#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace KDot
{
    // -------------------------------------------------------------------------
    // QualitySettings
    //
    //  The single "fidelity dial" for the engine. Every subsystem (terrain LOD,
    //  render distance, lighting, shadows, texture sampling) derives its
    //  parameters from one master value, m_Fidelity, in the range [0, 1].
    //
    //      0.0  -> Iruna Online tier  (tiny draw distance, coarse meshes)
    //      1.0  -> Elden Ring tier    (huge draw distance, dense meshes, shadows)
    //
    //  The project default is 0.65, the target look for K.Engine. Changing the
    //  dial at runtime recomputes every derived field, so a settings menu only
    //  ever has to touch SetFidelity()/ApplyTier().
    // -------------------------------------------------------------------------

    enum class QualityTier : int
    {
        Iruna   = 0, // 0.00 - mobile MMO baseline
        Low     = 1, // 0.25
        Medium  = 2, // 0.50
        KEngine = 3, // 0.65 - the project's target fidelity
        High    = 4, // 0.80
        Elden   = 5  // 1.00 - reference ceiling
    };

    struct QualitySettings
    {
        // ---- Derived parameters (read by other subsystems) ------------------
        float renderDistance        = 1350.0f; // far plane / world cull distance (world units)
        int   maxLODLevels          = 4;       // number of discrete LOD steps (LOD0..LODn-1)
        float lodDistanceStep       = 220.0f;  // world distance between successive LOD bands
        float lodBias               = 1.0f;    // global multiplier on LOD band distances (>1 keeps detail longer)
        int   terrainChunkEdgeVerts = 65;      // vertices per chunk edge at LOD0 (always 2^n + 1)
        bool  shadowsEnabled        = true;
        int   shadowMapSize         = 2048;    // 0 == disabled
        int   maxLights             = 24;
        int   anisotropy            = 8;       // texture anisotropic filtering samples
        float textureLodBias        = -0.15f;  // negative == sharper sampling
        int   msaaSamples           = 4;       // 1 == off
        // ---- PBR-style surface detail (normal / parallax / AO from textures) ---
        bool  normalMapping         = true;    // derived bump + authored normal maps
        int   parallaxSteps         = 16;      // displacement (POM) ray-march steps; 0 == off

        // ---- Master dial ----------------------------------------------------
        static QualitySettings& Get()
        {
            static QualitySettings s_Instance = [] {
                QualitySettings q;
                q.SetFidelity(0.65f); // K.Engine target
                return q;
            }();
            return s_Instance;
        }

        float Fidelity() const { return m_Fidelity; }

        // Set the master fidelity dial and recompute every derived field.
        void SetFidelity(float fidelity)
        {
            m_Fidelity = std::clamp(fidelity, 0.0f, 1.0f);
            const float f = m_Fidelity;

            renderDistance  = Lerp(120.0f, 2600.0f, f);
            maxLODLevels    = static_cast<int>(std::lround(Lerp(2.0f, 6.0f, f)));
            lodDistanceStep = Lerp(70.0f, 420.0f, f);
            lodBias         = Lerp(0.8f, 1.4f, f);

            // Chunk density snaps to 2^n + 1 so neighbouring LODs share edge verts.
            terrainChunkEdgeVerts = SnapPow2Plus1(static_cast<int>(Lerp(17.0f, 129.0f, f)));

            shadowsEnabled = f > 0.2f;
            shadowMapSize  = shadowsEnabled ? SnapPow2(static_cast<int>(Lerp(512.0f, 4096.0f, f))) : 0;

            maxLights      = static_cast<int>(std::lround(Lerp(4.0f, 64.0f, f)));
            anisotropy     = SnapPow2(static_cast<int>(Lerp(1.0f, 16.0f, f)));
            textureLodBias = Lerp(0.5f, -0.5f, f);
            msaaSamples    = SnapPow2(static_cast<int>(Lerp(1.0f, 8.0f, f)));

            // Derived bump is cheap (kept above Low); parallax/displacement is the
            // expensive ray-march, so it only switches on past the mid tier and
            // scales with fidelity. This is the "lock it behind a threshold" dial.
            normalMapping  = f > 0.25f;
            parallaxSteps  = (f > 0.35f) ? static_cast<int>(std::lround(Lerp(0.0f, 32.0f, f))) : 0;
        }

        // Convenience presets that map onto the same continuous dial.
        void ApplyTier(QualityTier tier)
        {
            switch (tier)
            {
                case QualityTier::Iruna:   SetFidelity(0.00f); break;
                case QualityTier::Low:     SetFidelity(0.25f); break;
                case QualityTier::Medium:  SetFidelity(0.50f); break;
                case QualityTier::KEngine: SetFidelity(0.65f); break;
                case QualityTier::High:    SetFidelity(0.80f); break;
                case QualityTier::Elden:   SetFidelity(1.00f); break;
            }
        }

        // Pick a LOD index from a camera-space distance. 0 == highest detail.
        // maxLevels lets callers cap against the resolution they actually have.
        int SelectLOD(float distance, int maxLevels) const
        {
            const int levels = std::max(1, std::min(maxLevels, maxLODLevels));
            const float band = std::max(1.0f, lodDistanceStep * lodBias);
            int lod = static_cast<int>(distance / band);
            return std::clamp(lod, 0, levels - 1);
        }

    private:
        float m_Fidelity = 0.65f;

        static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

        // Round up to the nearest power of two (clamped to >= 1).
        static int SnapPow2(int v)
        {
            if (v <= 1) return 1;
            int p = 1;
            while (p < v) p <<= 1;
            // Snap to whichever power of two is closer.
            return (p - v) < (v - (p >> 1)) ? p : (p >> 1);
        }

        // Snap to the nearest value of the form 2^n + 1 (17, 33, 65, 129 ...).
        static int SnapPow2Plus1(int v)
        {
            return SnapPow2(std::max(2, v - 1)) + 1;
        }
    };
}
