// SPDX-License-Identifier: MIT
#pragma once

#include "Entity.h"
#include "Scene.h"

#include <memory>
#include <string>
#include <unordered_map>
namespace DownPour {
typedef std::string                        str;
typedef std::unique_ptr<Scene>             ScenePtr;
typedef std::unique_ptr<Entity>            EntityPtr;
typedef std::unordered_map<str, ScenePtr>  SceneMap;
typedef std::unordered_map<str, EntityPtr> EntityMap;
typedef std::unordered_map<str, str>       EntityToSceneMap;

/**
 * @brief Manages multiple scenes and scene transitions
 *
 * Handles scene lifecycle, loading/unloading, and switching between
 * different game states (menu, garage, driving, etc.).
 * Also manages entities that belong to scenes.
 */
class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    // Scene management
    Scene* createScene(const str& name);
    Scene* getScene(const str& name);
    void   destroyScene(const str& name);

    // Active scene
    void         setActiveScene(const str& name);
    Scene*       getActiveScene();
    const Scene* getActiveScene() const;
    const str&   getActiveSceneName() const { return activeSceneName; }

    // Entity management (entities live in scenes)
    // PHASE 3: Support variadic arguments for entity constructors
    template <typename T = Entity, typename... Args>
    T* createEntity(const str& name, const str& sceneName, Args&&... args) {
        // Check if entity already exists (and is of correct type ideally, but we'll assume valid usage)
        if (entities.find(name) != entities.end())
            return dynamic_cast<T*>(entities[name].get());

        // Check if scene exists
        Scene* scene = getScene(sceneName);
        if (!scene)
            return nullptr;

        // Create entity with forwarded arguments
        auto entity         = std::make_unique<T>(name, scene, std::forward<Args>(args)...);
        T*   entityPtr      = entity.get();
        entities[name]      = std::move(entity);
        entityToScene[name] = sceneName;

        return entityPtr;
    }

    Entity* getEntity(const str& name) const;
    void    destroyEntity(const str& name);

    // Update
    void update(float deltaTime);  // Update transforms, animations

    // Cleanup
    void clear();

    // Query
    size_t getSceneCount() const { return scenes.size(); }
    size_t getEntityCount() const { return entities.size(); }

private:
    SceneMap         scenes;
    str              activeSceneName;
    EntityMap        entities;
    EntityToSceneMap entityToScene;  // Which scene owns which entity
};

}  // namespace DownPour
