#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

glm::vec3 Camera::getPosition() const {
    return pos;
}

glm::vec3 Camera::getTarget() const {
    return target;
}

glm::mat4 Camera::getProjectionMatrix(float aspect) const {
    glm::mat4 proj = glm::perspective(fovY, aspect, zNear, zFar);
    proj[1][1] *= -1;
    return proj;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(pos, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::moveCamera(glm::vec3 dirVector) {
    pos += dirVector;
    target += dirVector;
}

float Camera::getSpeed() const {
    return speed;
}
