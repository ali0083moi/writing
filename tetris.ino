
enum color {
	SSH110X_BLACK, SSH110X_WHITE, SSH110X_BLUE, SSH110X_GREEN, SSH110X_RED, SSH110X_PINK, SSH110_YELLOW
}

const bool O[4][4] = {
	{0 , 0 , 0 , 0},
	{0 , 1 , 1 , 0},
	{0 , 1 , 1 , 0},
	{0 , 0 , 0 , 0}
};

const bool I[4][4] = {
	{0 , 0 , 0 , 0},
	{1 , 1 , 1 , 1},
	{0 , 0 , 0 , 0},
	{0 , 0 , 0 , 0}
};

const bool L[4][4] = {
	{0 , 0 , 0 , 0},
	{1 , 1 , 1 , 0},
	{0 , 0 , 1 , 0},
	{0 , 0 , 0 , 0}
};

const bool J[4][4] = {
	{0 , 0 , 0 , 0},
	{1 , 1 , 1 , 0},
	{1 , 0 , 0 , 0},
	{0 , 0 , 0 , 0}
};

const bool T[4][4] = {
	{0 , 0 , 0 , 0},
	{1 , 1 , 1 , 0},
	{0 , 1 , 0 , 0},
	{0 , 0 , 0 , 0}
};

const bool z[4][4] = {
	{0 , 0 , 0 , 0},
	{1 , 1 , 0 , 0},
	{0 , 1 , 1 , 0},
	{0 , 0 , 0 , 0}
};

const bool s[4][4] = {
	{0 , 0 , 0 , 0},
	{0 , 1 , 1 , 0},
	{1 , 1 , 0 , 0},
	{0 , 0 , 0 , 0}
};

const bool*** shapes[7] = {
	O, I, L, J, T, z, s
};

int board[20][10];
int cur_top, cur_left , cur_color;
bool cur_shape[4][4];

void rotate() {
	bool temp[4][4];
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			temp[j][3 - i] = cur_shape[i][j];
		}
	}
	memcpy(cur_shape, temp, sizeof(cur_shape));
}

void draw_board() {
	for (int i = 0; i < 20; i++) {
		for (int j = 0; j < 10; j++) {
			display.fillRect(j * 6, i * 6, 6, 6, colour[board[i][j]]);
		}
	}
}

void new_shape(){
    for(int i = 0 ; i < 20 ; i++){
        bool complete_row = true;
        for(int j = 0 ; j < 10 ; j++){
            complete_row &= board[i][j];
        }
        if(complete_row){
            for(int j = 0 ; j < 10 ; j++){
                board[i][j] = 0;
            }
            for(int k = i ; k > 0 ; k--){
                memcpy(board[k], board[k-1], sizeof(board[k]));
            }
        }
    }
	memcpy(cur_shape, shapes[random(0, 7)], sizeof(cur_shape));
	cur_top = 0; cur_left = 0;
	cur_color = random(0, 7);
	return;
}

void next_frame(){
	for(int i = 0 ; i < 4 ; i++){
		for(int j = 0 ; j < 4 ; j++){
			if(cur_shape[i][j]){
				if(board[cur_top + i + 1][cur_left + j]){
					new_shape();
					return;
				}
			}
		}
	}
	for(int j = 0 ; j < 4 ; j++){
		board[cur_top][cur_left + j] = 0;
	}
	cur_top++; cur_left++;
	for(int i = 0 ; i < 4 ; i++){
		for(int j = 0 ; j < 4 ; j++){
			board[cur_top + i][cur_left + j] = cur_shape[i][j] * cur_color;
		}
	}
	return;
}

void display_board(){
	display.clearDisplay();
	draw_board();
	display.display();
}
