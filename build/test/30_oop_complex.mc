// Test 30: Complex OOP — Stack class built on top of an array + Counter
// Stack stores up to 8 ints; push/pop via index tracking
// Expected exit code: 60  (sum of popped values 10+20+30 = 60)

class Stack {
    int top;    // index of next free slot (0 = empty)

    void init() {
        this.top = 0;
    }

    int getTop() {
        return this.top;
    }

    void incTop() {
        this.top = this.top + 1;
    }

    void decTop() {
        this.top = this.top - 1;
    }
}

int main() {
    Stack s;
    s.init();

    // Simulate push: store values in local array, track top via object
    int data[8];

    // push 10
    data[s.getTop()] = 10;
    s.incTop();

    // push 20
    data[s.getTop()] = 20;
    s.incTop();

    // push 30
    data[s.getTop()] = 30;
    s.incTop();

    // pop all three and sum them
    int total;
    total = 0;

    int i;
    i = 0;
    while (i < 3) {
        s.decTop();
        total = total + data[s.getTop()];
        i++;
    }

    return total;   // 30+20+10 = 60
}
