; ModuleID = 'quail'
source_filename = "quail"

%MathHelper = type { i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @MathHelper_init(%MathHelper* nocapture writeonly %this_arg, i32 %0) local_unnamed_addr #0 {
entry:
  %this.value.ptr = getelementptr %MathHelper, %MathHelper* %this_arg, i64 0, i32 0
  store i32 %0, i32* %this.value.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @MathHelper_abs(%MathHelper* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.value.ptr = getelementptr %MathHelper, %MathHelper* %this_arg, i64 0, i32 0
  %value = load i32, i32* %this.value.ptr, align 4
  %0 = tail call i32 @llvm.abs.i32(i32 %value, i1 false)
  ret i32 %0
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @MathHelper_max(%MathHelper* nocapture readonly %this_arg, i32 %0) local_unnamed_addr #1 {
entry:
  %this.value.ptr = getelementptr %MathHelper, %MathHelper* %this_arg, i64 0, i32 0
  %value = load i32, i32* %this.value.ptr, align 4
  %gt = icmp sgt i32 %value, %0
  %value. = select i1 %gt, i32 %value, i32 %0
  ret i32 %value.
}

; Function Attrs: mustprogress nofree nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #2 {
entry:
  ret i32 15
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare i32 @llvm.abs.i32(i32, i1 immarg) #3

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #1 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #2 = { mustprogress nofree nosync nounwind readnone willreturn }
attributes #3 = { nofree nosync nounwind readnone speculatable willreturn }
