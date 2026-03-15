// Test 27: public/private access modifiers (parsed but not enforced)
// Expected exit code: 77

class BankAccount {
    private int balance;
    private int bonus;

    public void deposit(int amount) {
        this.balance = this.balance + amount;
    }

    public void setBonus(int b) {
        this.bonus = b;
    }

    public int getTotal() {
        return this.balance + this.bonus;
    }
}

int main() {
    BankAccount acct;
    acct.deposit(50);
    acct.deposit(20);
    acct.setBonus(7);
    return acct.getTotal();   // 50+20+7 = 77
}

