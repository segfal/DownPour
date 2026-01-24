// SPDX-License-Identifier: MIT
#include "SceneManager.h"

namespace DownPour {

SceneManager::SceneManager() : activeSceneName("") {}

SceneManager::~SceneManager() {
    clear();
}

Scene* SceneManager::createScene(const std::string& name) {
    // Check if scene already exists
    if (scenes.find(name) != scenes.end()) {
        return scenes[name].get();
    }

    // Create new scene
    auto   scene    = std::make_unique<Scene>(name);
    Scene* scenePtr = scene.get();
    scenes[name]    = std::move(scene);

    // If no active scene, make this one active
    if (activeSceneName.empty()) {
        activeSceneName = name;
    }

    return scenePtr;
}

Scene* SceneManager::getScene(const std::string& name) {
    auto it = scenes.find(name);
    if (it != scenes.end()) {
        return it->second.get();
    }
    return nullptr;
}

void SceneManager::destroyScene(const std::string& name) {
    // Remove all entities belonging to this scene
    std::vector<std::string> entitiesToRemove;
    for (const auto& [entityName, sceneName] : entityToScene) {
        if (sceneName == name) {
            entitiesToRemove.push_back(entityName);
        }
    }
    for (const auto& entityName : entitiesToRemove) {
        destroyEntity(entityName);
    }

    // Remove scene
    scenes.erase(name);

    // If this was the active scene, clear active scene
    if (activeSceneName == name) {
        activeSceneName.clear();
    }
}

void SceneManager::setActiveScene(const std::string& name) {
    // Check if scene exists
    if (scenes.find(name) != scenes.end()) {
        activeSceneName = name;
    }
}

Scene* SceneManager::getActiveScene() {
    if (activeSceneName.empty()) {
        return nullptr;
    }
    return getScene(activeSceneName);
}

const Scene* SceneManager::getActiveScene() const {
    if (activeSceneName.empty()) {
        return nullptr;
    }
    auto it = scenes.find(activeSceneName);
    if (it != scenes.end()) {
        return it->second.get();
    }
    return nullptr;
}

Entity* SceneManager::getEntity(const std::string& name) const {
    auto it = entities.find(name);
    if (it != entities.end()) {
        return it->second.get();
    }
    return nullptr;
}

void SceneManager::destroyEntity(const std::string& name) {
    entities.erase(name);
    entityToScene.erase(name);
}

void SceneManager::update(float deltaTime) {
    // Update active scene transforms
    Scene* activeScene = getActiveScene();
    if (activeScene) {
        activeScene->updateTransforms();
    }

    // TODO: Update animations, physics, etc.
}

void SceneManager::clear() {
    entities.clear();
    entityToScene.clear();
    scenes.clear();
    activeSceneName.clear();
}

}  // namespace DownPour
