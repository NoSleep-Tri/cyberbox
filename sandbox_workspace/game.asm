; ============================================================================
;  METEOR DODGER - Retro Arcade Game for CBVM-16 Emulator
;  Ngôn ngữ: Bytecode Assembly cho CyberBox Virtual Machine
; ============================================================================

; Khoi tao trang thai ban dau
init:
    LOAD R0 18          ; R0 = Player X (giua man hinh)
    LOAD R1 12          ; R1 = Player Y (gan day man hinh)

    LOAD R2 10          ; R2 = Meteor 1 X
    LOAD R3 1           ; R3 = Meteor 1 Y

    LOAD R4 25          ; R4 = Meteor 2 X
    LOAD R5 3           ; R5 = Meteor 2 Y

    LOAD R6 0           ; R6 = Diem so (Score)

game_loop:
    CLEAR

    ; 1. Ve phi thuyen nguoi choi (▲)
    DRAW R0 R1 ^

    ; 2. Ve cac thien thach dang roi (*, @)
    DRAW R2 R3 *
    DRAW R4 R5 @

    ; 3. Xuat hien thi khung hinh ra console
    RENDER

    ; 4. Nhan phim dieu khien
    INPUT R7
    CMP R7 97           ; 'a' (qua trai)
    JE move_left
    CMP R7 65           ; 'A'
    JE move_left
    CMP R7 100          ; 'd' (qua phai)
    JE move_right
    CMP R7 68           ; 'D'
    JE move_right
    CMP R7 113          ; 'q' (thoat)
    JE exit_game
    CMP R7 81           ; 'Q'
    JE exit_game
    JMP check_player_bounds

move_left:
    CMP R0 2
    JL check_player_bounds
    SUB R0 2
    JMP check_player_bounds

move_right:
    CMP R0 33
    JG check_player_bounds
    ADD R0 2

check_player_bounds:
    ; 5. Di chuyen thien thach roi xuong
    ADD R3 1
    ADD R5 1

    ; 6. Kiem tra va cham voi Thien thach 1
    CMP R3 R1
    JNE check_meteor_2
    CMP R2 R0
    JE collision_hit

check_meteor_2:
    ; Kiem tra va cham voi Thien thach 2
    CMP R5 R1
    JNE check_respawn
    CMP R4 R0
    JE collision_hit

check_respawn:
    ; 7. Reset thien thach 1 khi cham day
    CMP R3 13
    JL check_respawn_2
    LOAD R3 0
    RAND R2 32
    ADD R2 2
    ADD R6 10           ; Tang 10 diem

check_respawn_2:
    ; Reset thien thach 2 khi cham day
    CMP R5 13
    JL loop_delay
    LOAD R5 0
    RAND R4 32
    ADD R4 2
    ADD R6 10           ; Tang 10 diem

loop_delay:
    SLEEP 85            ; Delay ~12 khung hinh/giay
    JMP game_loop

collision_hit:
    CLEAR
    RENDER
    HALT

exit_game:
    HALT
