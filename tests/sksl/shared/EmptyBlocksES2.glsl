
out vec4 sk_FragColor;
uniform float unknownInput;
vec4 main() {
    vec4 color = vec4(0.0);
    for (int counter = 0;counter < 10; ++counter) {
    }
    for (int counter = 0;counter < 10; ++counter) {
    }
    for (int counter = 0;counter < 10; ++counter) {
    }
    if (unknownInput == 1.0) color.y = 1.0;
    if (unknownInput == 2.0) ; else color.w = 1.0;
    return color;
}
