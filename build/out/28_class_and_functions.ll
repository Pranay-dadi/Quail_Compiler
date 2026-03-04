; ModuleID = 'quail'
source_filename = "quail"

%Vector = type { i32, i32 }

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn writeonly
define void @Vector_set(%Vector* nocapture writeonly %this_arg, i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %this.x.ptr = getelementptr %Vector, %Vector* %this_arg, i64 0, i32 0
  store i32 %0, i32* %this.x.ptr, align 4
  %this.y.ptr = getelementptr %Vector, %Vector* %this_arg, i64 0, i32 1
  store i32 %1, i32* %this.y.ptr, align 4
  ret void
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Vector_getX(%Vector* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.x.ptr = getelementptr %Vector, %Vector* %this_arg, i64 0, i32 0
  %x = load i32, i32* %this.x.ptr, align 4
  ret i32 %x
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Vector_getY(%Vector* nocapture readonly %this_arg) local_unnamed_addr #1 {
entry:
  %this.y.ptr = getelementptr %Vector, %Vector* %this_arg, i64 0, i32 1
  %y = load i32, i32* %this.y.ptr, align 4
  ret i32 %y
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readonly willreturn
define i32 @Vector_dot(%Vector* nocapture readonly %this_arg, i32 %0, i32 %1) local_unnamed_addr #1 {
entry:
  %this.x.ptr = getelementptr %Vector, %Vector* %this_arg, i64 0, i32 0
  %x = load i32, i32* %this.x.ptr, align 4
  %mul = mul i32 %x, %0
  %this.y.ptr = getelementptr %Vector, %Vector* %this_arg, i64 0, i32 1
  %y = load i32, i32* %this.y.ptr, align 4
  %mul4 = mul i32 %y, %1
  %add = add i32 %mul4, %mul
  ret i32 %add
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @square(i32 %0) local_unnamed_addr #2 {
entry:
  %mul = mul i32 %0, %0
  ret i32 %mul
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @main() local_unnamed_addr #2 {
entry:
  ret i32 25
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn writeonly }
attributes #1 = { mustprogress nofree norecurse nosync nounwind readonly willreturn }
attributes #2 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
