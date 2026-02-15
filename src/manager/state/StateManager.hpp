#pragma once




struct State {
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void onEnter() = 0;
    virtual void onExit() = 0;
};

struct Event {
    std::string name;
    std::any data;
};



struct SteeringState : State {
    void update(float deltaTime) override {
        std::cout << "SteeringState update" << std::endl;
    }
    void render() override {
        std::cout << "SteeringState render" << std::endl;
    }
};

struct BrakingState : State {
    void update(float deltaTime) override {
        std::cout << "BrakingState update" << std::endl;
    }
    void render() override {
        std::cout << "BrakingState render" << std::endl;
    }
};

struct IdleState : State {
    void update(float deltaTime) override {
        std::cout << "IdleState update" << std::endl;
    }
    void render() override {
        std::cout << "IdleState render" << std::endl;
    }
};

struct TireState : State {
    void update(float deltaTime) override {
        std::cout << "TireState update" << std::endl;
    }
    void render() override {
        std::cout << "TireState render" << std::endl;
    }
};



struct CarState : State {
    SteeringState* steeringState;
    BrakingState* brakingState;
    IdleState* idleState;
    TireState* tireState;

    void update(float deltaTime) override {
        steeringState->update(deltaTime);
    }
    void render() override {
};
};
struct StateManager {};


