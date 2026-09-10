#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

glm::vec3 Camera::getPosition() const {
    return pos;
}

glm::vec3 Camera::getFront() const {
   glm::vec3 front(0.0);
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);
    return front;
}

void Camera::rotate(float deltaPitch, float deltaYaw) {
    pitch = glm::clamp(pitch + deltaPitch, -89.9f, 89.9f);
    yaw += deltaYaw;
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const {
    glm::mat4 proj = glm::perspective(fovY, aspect, zNear, zFar);
    proj[1][1] *= -1;
    return proj;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(pos, pos + getFront(), glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::moveCamera(glm::vec3 dirVector) {
    pos += dirVector;
}

float Camera::getSpeed() const {
    return speed;
}

float Camera::getLookSpeed() const {
    return lookSpeed;
}
