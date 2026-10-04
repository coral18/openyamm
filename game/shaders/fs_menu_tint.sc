$input v_texcoord0
#include "common.sh"
SAMPLER2D(s_texColor, 0);
uniform vec4 u_menuTint;
void main()
{
    gl_FragColor = texture2D(s_texColor, v_texcoord0) * u_menuTint;
}
