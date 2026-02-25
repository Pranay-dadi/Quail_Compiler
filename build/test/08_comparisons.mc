int main() {
    int x;
    int score;
    x = 7;
    score = 0;
    if (x == 7) {
        score = score + 1;
    }
    if (x != 8) {
        score = score + 2;
    }
    if (x >= 7) {
        score = score + 4;
    }
    if (x <= 7) {
        score = score + 8;
    }
    return score;
}
