// Test 24: Method calling another method via this.method()
// Expected exit code: 100  (double(50) where double calls this.get()*2)

class Counter {
    int count;

    void set(int v) {
        this.count = v;
    }

    int get() {
        return this.count;
    }

    int doubled() {
        return this.get() * 2;
    }
}

int main() {
    Counter c;
    c.set(50);
    int result = c.doubled();
    return result;
}

