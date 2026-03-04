; ModuleID = 'quail'
source_filename = "quail"

%BankAccount = type { i32, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn
define void @BankAccount_deposit(%BankAccount* nocapture %this_arg, i32 %0) local_unnamed_addr #0 {
entry:
  %this.balance.ptr = getelementptr %BankAccount, %BankAccount* %this_arg, i64 0, i32 0
  %balance = load i32, i32* %this.balance.ptr, align 4
  %add = add i32 %balance, %0
  store i32 %add, i32* %this.balance.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @BankAccount_setBonus(%BankAccount* nocapture writeonly %this_arg, i32 %0) local_unnamed_addr #1 {
entry:
  %this.bonus.ptr = getelementptr %BankAccount, %BankAccount* %this_arg, i64 0, i32 1
  store i32 %0, i32* %this.bonus.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @BankAccount_getTotal(%BankAccount* nocapture readonly %this_arg) local_unnamed_addr #2 {
entry:
  %this.balance.ptr = getelementptr %BankAccount, %BankAccount* %this_arg, i64 0, i32 0
  %balance = load i32, i32* %this.balance.ptr, align 4
  %this.bonus.ptr = getelementptr %BankAccount, %BankAccount* %this_arg, i64 0, i32 1
  %bonus = load i32, i32* %this.bonus.ptr, align 4
  %add = add i32 %bonus, %balance
  ret i32 %add
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #3 {
entry:
  ret i32 77
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn }
attributes #1 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #2 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #3 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
