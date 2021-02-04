
out vec4 sk_FragColor;
uniform vec4 colorGreen;
vec4 main() {
    vec4 result;
    result.x = (sqrt(1.0) , colorGreen.x);
    result.y = (vec2(2.0) , colorGreen.y);
    result.z = (vec3(3.0) , colorGreen.z);
    result.w = (mat2(4.0) , colorGreen.w);
    return result;
}
