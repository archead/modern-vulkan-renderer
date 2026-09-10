#pragma once
#include <glm/glm.hpp>

class Camera {
private:
    glm::vec3 pos = {0.0f, 0.0f, 2.0f};
    float speed = 2.0f; // units/s
    float lookSpeed = 90.0f; // deg/s
    float fovY= 45.0f;
    float zNear = 0.1f;
    float zFar = 50.0f;
    float pitch = 0.0f;
    float yaw = -90.0f;
public:
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspect) const;
    glm::vec3 getPosition() const;
    glm::vec3 getFront() const;
    void rotate(float deltaPitch, float deltaYaw);
    float getSpeed() const;
    float getLookSpeed() const;
    void moveCamera(glm::vec3 dirVector);

};