; ModuleID = 'quail'
source_filename = "quail"

%Counter = type { i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @Counter_set(%Counter* nocapture writeonly %this_arg, i32 %0) local_unnamed_addr #0 {
entry:
  %this.count.ptr = getelementptr %Counter, %Counter* %this_arg, i64 0, i32 0
  store i32 %0, i32* %this.count.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Counter_get(%Counter* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.count.ptr = getelementptr %Counter, %Counter* %this_arg, i64 0, i32 0
  %count = load i32, i32* %this.count.ptr, align 4
  ret i32 %count
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Counter_doubled(%Counter* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.count.ptr.i = getelementptr %Counter, %Counter* %this_arg, i64 0, i32 0
  %count.i = load i32, i32* %this.count.ptr.i, align 4
  %mul = shl i32 %count.i, 1
  ret i32 %mul
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #2 {
entry:
  ret i32 100
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #1 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #2 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
