
#version 330


// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matNormal;
uniform vec3 viewPos;

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;
out vec3 fragViewPos;

in mat4 instanceTransform;


void main()
{
    // Send vertex attributes to fragment shader
    fragPosition = vec3(instanceTransform * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vec4(1.0);
    fragNormal = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));

    fragViewPos = viewPos;

    // Calculate final vertex position
    gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
}
