// Test 29: Two different classes used together
// Expected exit code: 12  (rect.area() - circle.radius = 15 - 3 = 12)

class Rectangle {
    int width;
    int height;

    void init(int w, int h) {
        this.width  = w;
        this.height = h;
    }

    int area() {
        return this.width * this.height;
    }

    int perimeter() {
        return 2 * (this.width + this.height);
    }
}

class Circle {
    int radius;

    void setRadius(int r) {
        this.radius = r;
    }

    int getRadius() {
        return this.radius;
    }
}

int main() {
    Rectangle rect;
    rect.init(3, 5);        // area = 15

    Circle c;
    c.setRadius(3);

    int result = rect.area() - c.getRadius();   // 15 - 3 = 12
    return result;
}

