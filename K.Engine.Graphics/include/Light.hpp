namespace KDot
{
    class AmbientLight // Single ambient light
    {
        glm::vec3 color;
        float intensity;
    };
    class PointLight // Multiple point lights
    {
        glm::vec3 position;
        glm::vec3 color;
        float intensity;
        float radius;

    };
    class DirectionalLight // Single directional light
    {
        glm::vec3 direction;
        glm::vec3 color;
        float intensity;
    };
    class SpotLight // Multiple spot lights
    {
        glm::vec3 position;
        glm::vec3 direction;
        glm::vec3 color;
        float intensity;
        float radius;
        float angle;

        float innerAngle;
        float outerAngle;
        float constant;
        float linear;
        float quadratic;


    };
}