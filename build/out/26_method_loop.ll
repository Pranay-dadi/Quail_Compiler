; ModuleID = 'quail'
source_filename = "quail"

%Accumulator = type { i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @Accumulator_reset(%Accumulator* nocapture writeonly %this_arg) local_unnamed_addr #0 {
entry:
  %this.total.ptr = getelementptr %Accumulator, %Accumulator* %this_arg, i64 0, i32 0
  store i32 0, i32* %this.total.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn
define void @Accumulator_add(%Accumulator* nocapture %this_arg, i32 %0) local_unnamed_addr #1 {
entry:
  %this.total.ptr = getelementptr %Accumulator, %Accumulator* %this_arg, i64 0, i32 0
  %total = load i32, i32* %this.total.ptr, align 4
  %add = add i32 %total, %0
  store i32 %add, i32* %this.total.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Accumulator_result(%Accumulator* nocapture readonly %this_arg) local_unnamed_addr #2 {
entry:
  %this.total.ptr = getelementptr %Accumulator, %Accumulator* %this_arg, i64 0, i32 0
  %total = load i32, i32* %this.total.ptr, align 4
  ret i32 %total
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #3 {
entry:
  ret i32 55
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #1 = { mustprogress nofree norecurse nosync nounwind willreturn }
attributes #2 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #3 = { nofree norecurse nosync nounwind readnone }
