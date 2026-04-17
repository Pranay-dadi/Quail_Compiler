; ModuleID = 'quail'
source_filename = "quail"

@.str = private unnamed_addr constant [26 x i8] c"=== Quail scan() Demo ===\00", align 1
@.fmt = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@.str.2 = private unnamed_addr constant [37 x i8] c"Enter three integers (one per line):\00", align 1
@.fmt.3 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@.str.4 = private unnamed_addr constant [16 x i8] c"--- Results ---\00", align 1
@.str.5 = private unnamed_addr constant [4 x i8] c"a =\00", align 1
@.str.6 = private unnamed_addr constant [4 x i8] c"b =\00", align 1
@.str.7 = private unnamed_addr constant [4 x i8] c"c =\00", align 1
@.str.8 = private unnamed_addr constant [13 x i8] c"Sum        :\00", align 1
@.str.9 = private unnamed_addr constant [13 x i8] c"Product    :\00", align 1
@.str.10 = private unnamed_addr constant [13 x i8] c"Max(a,b)   :\00", align 1
@.str.11 = private unnamed_addr constant [13 x i8] c"Min(a,b)   :\00", align 1
@.str.12 = private unnamed_addr constant [13 x i8] c"Max of all :\00", align 1
@.str.13 = private unnamed_addr constant [13 x i8] c"|a - b|    :\00", align 1
@.str.14 = private unnamed_addr constant [8 x i8] c"Grade: \00", align 1
@.str.19 = private unnamed_addr constant [13 x i8] c"=== Done ===\00", align 1

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @max_of(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %gt = icmp sgt i32 %0, %1
  %common.ret.op = select i1 %gt, i32 %0, i32 %1
  ret i32 %common.ret.op
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone willreturn
define i32 @min_of(i32 %0, i32 %1) local_unnamed_addr #0 {
entry:
  %lt = icmp slt i32 %0, %1
  %common.ret.op = select i1 %lt, i32 %0, i32 %1
  ret i32 %common.ret.op
}

; Function Attrs: mustprogress nofree nosync nounwind readnone willreturn
define i32 @abs_val(i32 %0) local_unnamed_addr #1 {
entry:
  %1 = tail call i32 @llvm.abs.i32(i32 %0, i1 false)
  ret i32 %1
}

; Function Attrs: nofree nounwind
define i32 @main() local_unnamed_addr #2 {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %c = alloca i32, align 4
  %0 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str, i64 0, i64 0))
  %putchar = tail call i32 @putchar(i32 10)
  %putchar45 = tail call i32 @putchar(i32 10)
  %1 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([37 x i8], [37 x i8]* @.str.2, i64 0, i64 0))
  %putchar46 = tail call i32 @putchar(i32 10)
  %2 = call i32 (i8*, ...) @scanf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32* nonnull %a)
  %3 = call i32 (i8*, ...) @scanf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32* nonnull %b)
  %4 = call i32 (i8*, ...) @scanf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32* nonnull %c)
  %putchar47 = call i32 @putchar(i32 10)
  %5 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([16 x i8], [16 x i8]* @.str.4, i64 0, i64 0))
  %putchar48 = call i32 @putchar(i32 10)
  %6 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.5, i64 0, i64 0))
  %a1 = load i32, i32* %a, align 4
  %7 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %a1)
  %putchar49 = call i32 @putchar(i32 10)
  %8 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.6, i64 0, i64 0))
  %b2 = load i32, i32* %b, align 4
  %9 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %b2)
  %putchar50 = call i32 @putchar(i32 10)
  %10 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.7, i64 0, i64 0))
  %c3 = load i32, i32* %c, align 4
  %11 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %c3)
  %putchar51 = call i32 @putchar(i32 10)
  %putchar52 = call i32 @putchar(i32 10)
  %12 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str.8, i64 0, i64 0))
  %a4 = load i32, i32* %a, align 4
  %b5 = load i32, i32* %b, align 4
  %add = add i32 %b5, %a4
  %c6 = load i32, i32* %c, align 4
  %add7 = add i32 %add, %c6
  %13 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %add7)
  %putchar53 = call i32 @putchar(i32 10)
  %14 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str.9, i64 0, i64 0))
  %a8 = load i32, i32* %a, align 4
  %b9 = load i32, i32* %b, align 4
  %mul = mul i32 %b9, %a8
  %c10 = load i32, i32* %c, align 4
  %mul11 = mul i32 %mul, %c10
  %15 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %mul11)
  %putchar54 = call i32 @putchar(i32 10)
  %16 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str.10, i64 0, i64 0))
  %a12 = load i32, i32* %a, align 4
  %b13 = load i32, i32* %b, align 4
  %gt.i = icmp sgt i32 %a12, %b13
  %common.ret.op.i = select i1 %gt.i, i32 %a12, i32 %b13
  %17 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %common.ret.op.i)
  %putchar55 = call i32 @putchar(i32 10)
  %18 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str.11, i64 0, i64 0))
  %a14 = load i32, i32* %a, align 4
  %b15 = load i32, i32* %b, align 4
  %lt.i = icmp slt i32 %a14, %b15
  %common.ret.op.i69 = select i1 %lt.i, i32 %a14, i32 %b15
  %19 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %common.ret.op.i69)
  %putchar56 = call i32 @putchar(i32 10)
  %20 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str.12, i64 0, i64 0))
  %a17 = load i32, i32* %a, align 4
  %b18 = load i32, i32* %b, align 4
  %gt.i70 = icmp sgt i32 %a17, %b18
  %common.ret.op.i71 = select i1 %gt.i70, i32 %a17, i32 %b18
  %c20 = load i32, i32* %c, align 4
  %gt.i72 = icmp sgt i32 %common.ret.op.i71, %c20
  %common.ret.op.i73 = select i1 %gt.i72, i32 %common.ret.op.i71, i32 %c20
  %21 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %common.ret.op.i73)
  %putchar57 = call i32 @putchar(i32 10)
  %22 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str.13, i64 0, i64 0))
  %a22 = load i32, i32* %a, align 4
  %b23 = load i32, i32* %b, align 4
  %sub = sub i32 %a22, %b23
  %23 = call i32 @llvm.abs.i32(i32 %sub, i1 false) #4
  %24 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt.3, i64 0, i64 0), i32 %23)
  %putchar58 = call i32 @putchar(i32 10)
  %putchar59 = call i32 @putchar(i32 10)
  %a25 = load i32, i32* %a, align 4
  %b26 = load i32, i32* %b, align 4
  %add27 = add i32 %b26, %a25
  %c28 = load i32, i32* %c, align 4
  %add29 = add i32 %add27, %c28
  %25 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([8 x i8], [8 x i8]* @.str.14, i64 0, i64 0))
  %ge = icmp sgt i32 %add29, 89
  br i1 %ge, label %ifcont.thread, label %ifcont

ifcont.thread:                                    ; preds = %entry
  %putchar67 = call i32 @putchar(i32 65)
  %putchar68 = call i32 @putchar(i32 10)
  br label %ifcont35.thread

ifcont:                                           ; preds = %entry
  %ge32 = icmp sgt i32 %add29, 74
  br i1 %ge32, label %ifcont35.thread, label %ifcont35

ifcont35.thread:                                  ; preds = %ifcont, %ifcont.thread
  %putchar65 = call i32 @putchar(i32 66)
  %putchar66 = call i32 @putchar(i32 10)
  br label %ifcont40

ifcont35:                                         ; preds = %ifcont
  %ge37 = icmp sgt i32 %add29, 59
  %spec.select = select i1 %ge37, i32 67, i32 70
  br label %ifcont40

ifcont40:                                         ; preds = %ifcont35, %ifcont35.thread
  %.sink = phi i32 [ 67, %ifcont35.thread ], [ %spec.select, %ifcont35 ]
  %putchar60 = call i32 @putchar(i32 %.sink)
  %putchar61 = call i32 @putchar(i32 10)
  %26 = call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([3 x i8], [3 x i8]* @.fmt, i64 0, i64 0), i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str.19, i64 0, i64 0))
  %putchar62 = call i32 @putchar(i32 10)
  ret i32 %add29
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare noundef i32 @scanf(i8* nocapture noundef readonly, ...) local_unnamed_addr #2

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare i32 @llvm.abs.i32(i32, i1 immarg) #3

; Function Attrs: nofree nounwind
declare noundef i32 @putchar(i32 noundef) local_unnamed_addr #2

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn }
attributes #1 = { mustprogress nofree nosync nounwind readnone willreturn }
attributes #2 = { nofree nounwind }
attributes #3 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #4 = { nounwind }
