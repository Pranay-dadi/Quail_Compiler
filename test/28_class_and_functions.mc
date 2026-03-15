// Test 28: Class + free function interoperability
// Expected exit code: 25  (square(p.getX()) = 5*5)

class Vector {
    int x;
    int y;

    void set(int nx, int ny) {
        this.x = nx;
        this.y = ny;
    }

    int getX() { return this.x; }
    int getY() { return this.y; }

    int dot(int ox, int oy) {
        return this.x * ox + this.y * oy;
    }
}

int square(int n) {
    return n * n;
}

int main() {
    Vector v;
    v.set(3, 4);

    // Use free function with object data
    int sx = square(v.getX());    // 9
    int sy = square(v.getY());    // 16
    int magnitude = sx + sy;      // 25  (3^2 + 4^2 = Pythagoras)
    return magnitude;
}

