// Zero disables clipping. Reflection passes keep geometry on the camera's side of the water plane.
uniform vec4 u_worldClipPlane;

void clipWorldPosition(vec3 worldPosition)
{
    if (dot(vec4(worldPosition, 1.0), u_worldClipPlane) < 0.0)
    {
        discard;
    }
}
