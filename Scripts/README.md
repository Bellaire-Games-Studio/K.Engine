# Scripts/ — user-authored C++ behaviours

Game logic in K.Engine is written as C++ **behaviours**, not picked from a fixed
list of presets. Any `.cpp` you drop in this folder is compiled into the engine
and its behaviour becomes selectable in the editor.

## Write one

1. Open the editor and choose **Scripts → C++ Script Editor** (or just create a
   file here by hand).
2. Click **New from template**, give it a class name, edit `OnUpdate`, and
   **Save to Scripts/**. You get a file like `Scripts/MyBehaviour.cpp`:

   ```cpp
   #include <Script/ScriptBehavior.hpp>
   #include <gtc/quaternion.hpp>

   namespace KDot
   {
       class MyBehaviour : public ScriptBehavior
       {
       public:
           void OnInspect(ScriptParams& p) override { p.Float("speed", m_Speed, 0, 360); }
           void OnUpdate(float dt) override
           {
               if (Transform* t = GetTransform())
                   t->rotation = glm::angleAxis(glm::radians(m_Speed * dt),
                                                glm::vec3(0, 1, 0)) * t->rotation;
           }
       private:
           float m_Speed = 45.0f;
       };
       KE_REGISTER_SCRIPT(MyBehaviour, "MyBehaviour")
   }
   ```

3. **Rebuild** (re-run CMake so the new file is globbed in, then build). On
   native desktop this is the live author-and-rebuild loop; the browser build
   can't compile at runtime, so web picks new scripts up on the next CMake build.
4. In the editor, add a **Script** component to an entity and pick your
   behaviour from the drop-down. Press **Play** to run it.

## What you can do in a behaviour

- `OnStart()` — once when Play begins. `OnUpdate(float dt)` — every simulated frame.
- `OnInspect(ScriptParams&)` — declare `Float/Int/Bool/Vec3` fields; they show in
  the inspector and serialize with the scene.
- `GetTransform()`, `Get<T>()`, `Scene()` (the ECS registry), `Self()`,
  `WorldPosition()`, `WorldMatrix()` — see `K.Engine.Editor/include/Script/ScriptBehavior.hpp`.

`Pulse.cpp` here is a complete worked example (it breathes an entity's scale).
