// Test 22: Class with multiple fields — Point struct area
// Expected exit code: 30  (x * y = 5 * 6)

class Point {
    int x;
    int y;

    int getX() {
        return this.x;
    }

    int getY() {
        return this.y;
    }

    int area() {
        return this.x * this.y;
    }
}

int main() {
    Point p;
    p.x = 5;
    p.y = 6;
    int result = p.area();
    return result;
}
