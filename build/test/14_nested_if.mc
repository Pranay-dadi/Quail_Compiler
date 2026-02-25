int grade(int score) {
    if (score >= 90) {
        return 4;
    } else {
        if (score >= 75) {
            return 3;
        } else {
            if (score >= 60) {
                return 2;
            } else {
                return 1;
            }
        }
    }
}

int main() {
    int g;
    g = grade(82);
    return g;
}
