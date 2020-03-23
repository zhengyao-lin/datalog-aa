; desired pts-to relation
; %a -> mem %a
; %b -> mem %b
; mem %b -> mem %a
; %c -> mem %a

declare i32 @what()

define i32 @main() {
entry:
    %a = alloca i32
    %b = alloca i32*

    store i32* %a, i32** %b
    %c = load i32*, i32** %b

    call i32 @what()

    ret i32 0
}
