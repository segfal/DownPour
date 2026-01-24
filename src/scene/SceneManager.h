// SPDX-License-Identifier: MIT
#pragma once

#include "Entity.h"
#include "Scene.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace DownPour {

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
    Scene* createScene(const std::string& name);
    Scene* getScene(const std::string& name);
    void   destroyScene(const std::string& name);

    // Active scene
    void               setActiveScene(const std::string& name);
    Scene*             getActiveScene();
    const Scene*       getActiveScene() const;
    const std::string& getActiveSceneName() const { return activeSceneName; }

    // Entity management (entities live in scenes)
    template <typename T = Entity>
    T* createEntity(const std::string& name, const std::string& sceneName) {
        // Check if entity already exists (and is of correct type ideally, but we'll assume valid usage)
        if (entities.find(name) != entities.end()) {
            return dynamic_cast<T*>(entities[name].get());
        }

        // Check if scene exists
        Scene* scene = getScene(sceneName);
        if (!scene) {
            return nullptr;
        }

        // Create entity
        auto entity         = std::make_unique<T>(name, scene);
        T*   entityPtr      = entity.get();
        entities[name]      = std::move(entity);
        entityToScene[name] = sceneName;

        return entityPtr;
    }

    Entity* getEntity(const std::string& name) const;
    void    destroyEntity(const std::string& name);

    // Update
    void update(float deltaTime);  // Update transforms, animations

    // Cleanup
    void clear();

    // Query
    size_t getSceneCount() const { return scenes.size(); }
    size_t getEntityCount() const { return entities.size(); }

private:
    std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;
    std::string                                             activeSceneName;

    std::unordered_map<std::string, std::unique_ptr<Entity>> entities;
    std::unordered_map<std::string, std::string>             entityToScene;  // Which scene owns which entity
};

}  // namespace DownPour
