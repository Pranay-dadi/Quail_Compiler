int main() {
    int i;
    int sum;
    
    sum = 0;
    
    for (i = 1; i <= 20; i++) {
        if (i > 10) {
            break;
        }
        sum = sum + i;
    }
    
    return sum;    
}