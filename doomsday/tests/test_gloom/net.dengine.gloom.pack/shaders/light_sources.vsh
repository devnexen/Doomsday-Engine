#version 330 core

uniform mat4 uCameraMvpMatrix;
uniform mat4 uModelViewMatrix;
uniform mat3 uWorldToViewRotate;

DENG_ATTRIB vec4  aVertex;
DENG_ATTRIB float aUV;
DENG_ATTRIB vec3  aOrigin;
DENG_ATTRIB vec3  aIntensity;
DENG_ATTRIB vec3  aDirection;
DENG_ATTRIB float aIndex;

flat DENG_VAR vec3  vOrigin;     // view space
flat DENG_VAR vec3  vDirection;  // view space
flat DENG_VAR vec3  vIntensity;
flat DENG_VAR float vRadius;
flat DENG_VAR int   vShadowIndex;

void main(void) {
    vRadius = aUV;
    vShadowIndex = floatBitsToInt(aIndex);

    // Position each instance at its origin.
    gl_Position = uCameraMvpMatrix * vec4(aOrigin + vRadius * aVertex.xyz, 1.0);

    vec4 origin = uModelViewMatrix * vec4(aOrigin, 1.0);
    vOrigin    = origin.xyz / origin.w;
    vDirection = uWorldToViewRotate * aDirection;
    vIntensity = aIntensity;
}
