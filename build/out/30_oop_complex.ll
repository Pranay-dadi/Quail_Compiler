; ModuleID = 'quail'
source_filename = "quail"

%Stack = type { i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @Stack_init(%Stack* nocapture writeonly %this_arg) local_unnamed_addr #0 {
entry:
  %this.top.ptr = getelementptr %Stack, %Stack* %this_arg, i64 0, i32 0
  store i32 0, i32* %this.top.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Stack_getTop(%Stack* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.top.ptr = getelementptr %Stack, %Stack* %this_arg, i64 0, i32 0
  %top = load i32, i32* %this.top.ptr, align 4
  ret i32 %top
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn
define void @Stack_incTop(%Stack* nocapture %this_arg) local_unnamed_addr #2 {
entry:
  %this.top.ptr = getelementptr %Stack, %Stack* %this_arg, i64 0, i32 0
  %top = load i32, i32* %this.top.ptr, align 4
  %add = add i32 %top, 1
  store i32 %add, i32* %this.top.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn
define void @Stack_decTop(%Stack* nocapture %this_arg) local_unnamed_addr #2 {
entry:
  %this.top.ptr = getelementptr %Stack, %Stack* %this_arg, i64 0, i32 0
  %top = load i32, i32* %this.top.ptr, align 4
  %sub = add i32 %top, -1
  store i32 %sub, i32* %this.top.ptr, align 4
  ret void
}

; Function Attrs: nofree norecurse nosync nounwind readnone
define i32 @main() local_unnamed_addr #3 {
entry:
  ret i32 60
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #1 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #2 = { mustprogress nofree norecurse nosync nounwind willreturn }
attributes #3 = { nofree norecurse nosync nounwind readnone }
