#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

struct win32_offscreen_buffer {
	BITMAPINFO info;
	void *memory;
	int width;
	int height;
	int bytes_per_pixel;
	int pitch;
};

struct window_dimension {
	int width;
	int height;
};

struct read_file_result {
	unsigned int contents_size;
	void *contents;
};

struct bitmap_result {
	BITMAPFILEHEADER *file_header;
	BITMAPINFOHEADER *info_header;
	unsigned int *pixels;
	unsigned int stride;
};

struct card_image {
	bitmap_result bmp;
	char path[50];
};

struct hand_rank_image {
	bitmap_result bmp;
};

struct game_assets {
	card_image card_images[52];
	hand_rank_image hrank_images[10];
};

struct card {
	int value;
	char suit;
	bool removed_from_deck;
};

struct hand {
	card cards[5];
	unsigned char folded;  // BOOL
};

struct deck_info {
	card cards[52];
};

struct coordinate {
	int x;
	int y;
};

enum hand_status {
	PreFlop,
	Flop,
	Turn,
	River,
	Showdown
};

enum hand_rank {
	StraightFlush  = 1,
	Quads          = 2,
	FullHouse      = 3,
	Flush          = 4,
	Straight       = 5,
	ThreeOfAKind   = 6,
	TwoPair        = 7,
	OnePair        = 8,
	HighCard       = 9
};

struct game_state {
	hand hands[2];
	deck_info deck;
	coordinate text_pos;
	hand_status status;
	hand_rank ranks[2];
	int flags;
};

/*
struct player {
	unsigned int stack;
	card hand[2];
};

struct game_state {
	hand_status status;
	player players[2];
	card board[5];
};*/

// FLAGS
int HAND_INITIALIZED = 1 << 0;

// GLOBALS
win32_offscreen_buffer global_buffer;
game_assets global_assets;
bool should_quit = false;
unsigned char flicker = 0;  // BOOL
// unsigned int frame_counter = 0;
game_state G_STATE = {};

HDC device_context;

char *suits = "hdcs";
int sorted_wheel_values[5] = { 2, 3, 4, 5, 14 };
// int proper_wheel_values[5] = { 1, 2, 3, 4, 5  };

card extract_random_card(card *deck) {
	int selector = rand() % 52;
	
	// WARNING: Will result in infinite loop if ALL cards have been removed from deck. Keep in mind during testing.
	// MAYBE TODO: Use assert here to make sure the deck is not empty.
	// Also maybe make deck a struct w/ a length property?
	while (deck[selector].removed_from_deck) {
		selector = rand() % 52;
	}
	
	card c;
	c = deck[selector];
	deck[selector].removed_from_deck = true;
	return c;
}

void test_hand_generator(card *hand, card *deck) {
	for (int i = 0; i < 5; i++) {
		hand[i] = extract_random_card(deck);
	}
}

void debug_print_hand(card *hand) {
	for (int i = 0; i < 5; i++) {
		char buffer[3];
		sprintf(buffer, "%d%c", hand[i].value, hand[i].suit);
		OutputDebugStringA(buffer);
	}
	OutputDebugStringA("\n");
}

void swap(card *hand, int i, int j) {
	card temp = hand[i];
	hand[i] = hand[j];
	hand[j] = temp;
}

void sort_hand(card *hand) {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4 - i; j++) {
			if (hand[j].value > hand[j + 1].value) {
				swap(hand, j, j + 1);
			}
		}
	}
}

// Input hand is expected to be sorted.
void aces_high_unless_wheel(card* hand) {
	bool is_wheel = true;
	for (int i = 0; i < 5; i++) {
		if (hand[i].value != sorted_wheel_values[i]) {
			is_wheel = false;
		}
	}
	
	if (is_wheel) {
		card output_hand[5];
		output_hand[0].value = 1;
		output_hand[0].suit = hand[4].suit;
		
		for (int i = 1; i < 5; i++) {
			output_hand[i] = hand[i - 1];
		}
		
		for (int i = 0; i < 5; i++) {
			hand[i] = output_hand[i];
		}
	}
}

// Expects input hand to be sorted and aces_high_unless_wheel'ed.
bool is_straight(card *hand) {
	for (int i = 0; i < 4; i++) {
		if (hand[i + 1].value - hand[i].value != 1) return false;
	}
	
	return true;
}

void debug_print_hand_rank(hand_rank rank) {
	switch (rank) {
		case StraightFlush:
			OutputDebugStringA("StraightFlush\n");
			break;
		case Quads:
			OutputDebugStringA("Quads\n");
			break;
		case FullHouse:
			OutputDebugStringA("FullHouse\n");
			break;
		case Flush:
			OutputDebugStringA("Flush\n");
			break;
		case Straight:
			OutputDebugStringA("Straight\n");
			break;
		case ThreeOfAKind:
			OutputDebugStringA("ThreeOfAKind\n");
			break;
		case TwoPair:
			OutputDebugStringA("TwoPair\n");
			break;
		case OnePair:
			OutputDebugStringA("OnePair\n");
			break;
		case HighCard:
			OutputDebugStringA("HighCard\n");
			break;
		default:
			OutputDebugStringA("\nInvalid hand rank.\n");
	}
}

char* get_hand_rank_filename(hand_rank rank) {
	switch (rank) {
		case StraightFlush:
			return "c:/projects/native_poker/assets/straight-flush.bmp";
			break;
		case Quads:
			return "c:/projects/native_poker/assets/quads.bmp";
			break;
		case FullHouse:
			return "c:/projects/native_poker/assets/full-house.bmp";
			break;
		case Flush:
			return "c:/projects/native_poker/assets/flush.bmp";
			break;
		case Straight:
			return "c:/projects/native_poker/assets/straight.bmp";
			break;
		case ThreeOfAKind:
			return "c:/projects/native_poker/assets/three-of-a-kind.bmp";
			break;
		case TwoPair:
			return "c:/projects/native_poker/assets/two-pair.bmp";
			break;
		case OnePair:
			return "c:/projects/native_poker/assets/one-pair.bmp";
			break;
		case HighCard:
			return "c:/projects/native_poker/assets/high-card.bmp";
			break;
		default:
			return "c:/projects/native_poker/assets/invalid-hand-rank.bmp";
	}
}

char* stringify_hand_rank(hand_rank rank) {
	switch (rank) {
		case StraightFlush:
			return "StraightFlush";
			break;
		case Quads:
			return "Quads";
			break;
		case FullHouse:
			return "FullHouse";
			break;
		case Flush:
			return "Flush";
			break;
		case Straight:
			return "Straight";
			break;
		case ThreeOfAKind:
			return "ThreeOfAKind";
			break;
		case TwoPair:
			return "TwoPair";
			break;
		case OnePair:
			return "OnePair";
			break;
		case HighCard:
			return "HighCard";
			break;
		default:
			return "Invalid hand rank.";
	}
}

hand_rank evaluate_hand(card *hand) {
	sort_hand(hand);
	aces_high_unless_wheel(hand);
	
	bool is_flush = true;
	char suit = hand[0].suit;
	for (int i = 1; i < 5; i++) {
		if (hand[i].suit != suit) {
			is_flush = false;
		}
	}
	
	bool is_str8 = is_straight(hand);
	if (is_flush) {
		if (is_str8) return StraightFlush;
		else return Flush;
	}
	
	if (is_str8) return Straight;
	
	{
		// Quads
		int x = hand[0].value;
		int quads_counter = 1;
		for (int i = 1; i < 4; i++) {
			if (hand[i].value == x) {
				quads_counter++;
				if (quads_counter == 4) return Quads;
			}
		}
		
		x = hand[1].value;
		quads_counter = 1;
		for (int i = 2; i < 5; i++) {
			if (hand[i].value == x) {
				quads_counter++;
				if (quads_counter == 4) return Quads;
			}
		}
	}
	
	{
		// FullHouse
		int running_index  = 1;
		int current_value = hand[0].value;
		int count = 1;
		
		for (int i = running_index; i < 3; i++) {
			if (hand[i].value == current_value) {
				count++;
				running_index++;
			} else {
				break;
			}
		}
		
		if (count >= 2) {
			current_value = hand[running_index].value;
			
			for (int i = running_index; i < 5; i++) {
				if (hand[i].value == current_value) {
					if (i == 4) return FullHouse;
				} else {
					break;
				}
			}
		}
	}
	
	{
		// ThreeOfAKind
		for (int i = 0; i < 3; i++) {
			int current_value = hand[i].value;
			int count = 1;
			
			for (int j = i + 1; j < 5; j++) {
				if (hand[j].value == current_value) count++;
				else break;
			}
			
			if (count == 3) return ThreeOfAKind;
		}
	}
	
	int pairs_count = 0;
	for (int i = 1; i < 5; i++) {
		if (hand[i].value == hand[i - 1].value) pairs_count++;
	}
	
	if (pairs_count == 1)       return OnePair;
	else if (pairs_count == 2)  return TwoPair;
	
	return HighCard;
}

void create_deck(card *deck) {
	int i = 0;
	for (int j = 0; j < 4; j++) {
		for (int k = 2; k <= 14; k++) {
			card c;
			c.value = k;
			c.suit = suits[j];
			c.removed_from_deck = false;
			deck[i] = c;
			i++;
		}
	}
}

bool test_deck_empty(card *deck) {
	int card_count = 0;
	for (int i = 0; i < 52; i++) {
		if (!deck[i].removed_from_deck) {
			card_count++;
		}
	}
	
	if (card_count < 5) return true;
	else return false;
}

uint32_t safe_truncate_uint64(uint64_t value) {
	// assert(value <= 0xFFFFFFFF);
	uint32_t result = (uint32_t)value;
	return result;
}

void free_file_memory(void *memory) {
	if (memory) {
		VirtualFree(memory, 0, MEM_RELEASE);
	}
}

read_file_result read_entire_file(LPCSTR filename) {
	read_file_result result = {};
	
	HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	
	if (file_handle != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER file_size;
		if(GetFileSizeEx(file_handle, &file_size)) {
			uint32_t file_size32 = safe_truncate_uint64(file_size.QuadPart);
			result.contents = VirtualAlloc(0, file_size32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			
			if (result.contents) {
				DWORD bytes_read;
				if (ReadFile(file_handle, result.contents, file_size32, &bytes_read, 0) && (file_size32 == bytes_read)) {
					// File read successfully.
					result.contents_size = file_size32;
				} else {
					// TODO: Logging
					free_file_memory(result.contents);
					result.contents = 0;
				}
			} else {
				// TODO: Logging
			}
		} else {
			// TODO: Logging
		}
		
		CloseHandle(file_handle);
	} else {
		// TODO: Logging
	}
	
	return result;
}

window_dimension get_window_dimension(HWND window) {
	RECT client_rect;
	GetClientRect(window, &client_rect);
	
	window_dimension result;
	
	result.width = client_rect.right - client_rect.left;
	result.height = client_rect.bottom - client_rect.top;
	
	/* char dbug_str[50];
	sprintf(dbug_str, "%d, %d", result.width, result.height);
	OutputDebugStringA(dbug_str);
	OutputDebugStringA("\n"); */
	
	return result;
}

void resize_dib_section(win32_offscreen_buffer* buffer, int width, int height) {
	if (buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}
	
	int bytes_per_pixel = 4;
	
	buffer->width = width;
	buffer->height = height;
	
	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;
	
	int bitmap_memory_size = (buffer->width * buffer->height) * bytes_per_pixel;
	buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
	
	buffer->pitch = buffer->width * bytes_per_pixel;
	buffer->bytes_per_pixel = bytes_per_pixel;
}

void display_buffer_in_window(HDC device_context, window_dimension dimension) {
	StretchDIBits(device_context,
				  0, 0, dimension.width, dimension.height,
				  0, 0, global_buffer.width, global_buffer.height,
				  global_buffer.memory,
				  &global_buffer.info,
				  DIB_RGB_COLORS, SRCCOPY);
}

bitmap_result debug_load_bitmap(char* filename) {
	bitmap_result bmp_result = {};

	read_file_result file_result = read_entire_file(filename);
	unsigned char *contents = (unsigned char *)file_result.contents;

	bmp_result.file_header = (BITMAPFILEHEADER *)contents;
	bmp_result.info_header = (BITMAPINFOHEADER *)(contents + 14);
	bmp_result.pixels = (unsigned int *)(contents + bmp_result.file_header->bfOffBits);
	bmp_result.stride = ((((bmp_result.info_header->biWidth * bmp_result.info_header->biBitCount) + 31) & ~31) >> 3);
	// bmp_result.stride = 147;
	// bmp_result.stride = bmp_result.info_header->biWidth * 3;

	return bmp_result;
}

void debug_paint_window(unsigned int color) {
	// unsigned int red = 0xFF0000;
	unsigned int *pixel = (unsigned int *)global_buffer.memory; 
	
	for (int i = 0; i < global_buffer.height; i++) {
		for (int j = 0; j < global_buffer.width; j++) {
			*pixel++ = color;
		}
	}
}

void get_filename_for_card(char *buffer, card c) {
	char value[3];

	if (c.value < 10) {
		sprintf(value, "0%d", c.value);
	} else if (c.value == 14) {
		sprintf(value, "01");
	} else {
		sprintf(value, "%d", c.value);
	}

	sprintf(buffer, "c:/projects/native_poker/card-BMPs/%c%s.bmp", c.suit, value);
}

void load_card_bitmaps(card* deck) {
	for (int i = 0; i < 52; i++) {
		char path[50];
		get_filename_for_card(path, deck[i]);

		// global_assets.bitmaps 
	}
}

void render_bmp(int x_pos, int y_pos, win32_offscreen_buffer *buffer, bitmap_result bmp) {
	int width = bmp.info_header->biWidth;
	int height = bmp.info_header->biHeight;

	unsigned char* dest_row = (unsigned char*)buffer->memory + (y_pos * buffer->pitch + x_pos);

	// NOTE: Doing this calculation on the source row because the bitmaps are bottom up,
	// whereas the window is top-down. So must start at the bottom of the source bitmap,
	// working left to right.
	unsigned char* source_row = (unsigned char*)(bmp.pixels + ((bmp.stride / 4) * (height - 1)));
	
	for (int y = y_pos; y < y_pos + height; y++) {
		unsigned char *dest = dest_row;
		unsigned char *source = source_row;
		
		for (int x = x_pos; x < x_pos + width; x++) {
			for (int i = 0; i < 3; i++) {
				*dest = *source;
				dest++;
				source++;
			}
			
			*dest = 0x00;
			dest++;
		}

		dest_row += buffer->pitch;
		source_row -= bmp.stride;
	}
}

void update_and_render(game_state *G_STATE, win32_offscreen_buffer *buffer, game_assets *assets, HDC device_context) {
	if (test_deck_empty(G_STATE->deck.cards)) {
		create_deck(G_STATE->deck.cards);
	}
	
	if (!(G_STATE->flags & HAND_INITIALIZED)) {
		srand(time(0));
	
		test_hand_generator(G_STATE->hands[0].cards, G_STATE->deck.cards);
		test_hand_generator(G_STATE->hands[1].cards, G_STATE->deck.cards);
		
		G_STATE->ranks[0] = evaluate_hand(G_STATE->hands[0].cards);
		debug_print_hand_rank(G_STATE->ranks[0]);
		
		G_STATE->flags |= HAND_INITIALIZED;
	}
	
	int x = 0;
	int y = 0;
	for (int i = 0; i < 5; i++) {
		char path[50];
		get_filename_for_card(path, G_STATE->hands[0].cards[i]);
		
		for (int j = 0; j < 52; j++) {
			if (strcmp(global_assets.card_images[j].path, path) == 0) {
				render_bmp(x, y, buffer, global_assets.card_images[j].bmp);
				
				x += (global_assets.card_images[j].bmp.info_header->biWidth) * 4;
				// y += global_assets.card_images[j].bmp.info_header->biHeight;
			}
		}
	}
	
	render_bmp(750, 250, buffer, global_assets.hrank_images[G_STATE->ranks[0]].bmp);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	LRESULT result = 0;

	switch (message) {
		break;
		case WM_SIZE: {
			// window_dimension dim = get_window_dimension(window);
			// resize_dib_section(&global_buffer, dim.width, dim.height);
		}
		break;

		case WM_CLOSE: {
			OutputDebugStringA("WM_CLOSE\n");
			should_quit = true;
		}
		break;

		case WM_ACTIVATEAPP: {
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		}
		break;

		case WM_DESTROY: {
			OutputDebugStringA("WM_DESTROY\n");
		}
		break;

		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			
			window_dimension dimension = get_window_dimension(window);
			display_buffer_in_window(device_context, dimension);
			
			OutputDebugStringA("WM_PAINT\n");

			EndPaint(window, &paint);
		}
		break;
		
		case WM_SETFONT: {
			OutputDebugStringA("WM_SETFONT\n");
		}
		break;
	
		case WM_KEYDOWN: {
			OutputDebugStringA("KEY DOWN\n");
			
			if (w_param == VK_RETURN) {
				G_STATE.flags &= ~HAND_INITIALIZED;
			}
		}
		break;

		default: {
			result = DefWindowProc(window, message, w_param, l_param);
		}
		break;
	}

	return result;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int show_code) {
	WNDCLASS window_class = {};

	window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.lpfnWndProc = window_proc;
	window_class.hInstance = instance;
	window_class.lpszClassName = "PokerWindowClass";
	
	if (RegisterClassA(&window_class)) {
		HWND window_handle = CreateWindowExA(0, window_class.lpszClassName, "Poker",
			                          WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
									  CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

		if (window_handle) {
			device_context = GetDC(window_handle);
			
			window_dimension dim = get_window_dimension(window_handle);
			resize_dib_section(&global_buffer, dim.width, dim.height);
			
			create_deck(G_STATE.deck.cards);
			
			// LOAD ASSETS
			
			// Set first one to null so we can index into the array using the hand rank enum values
			hand_rank_image null_hrank_img;
			null_hrank_img = {};
			global_assets.hrank_images[0] = null_hrank_img;
			for (int i = 1; i <= 9; i++) {
				hand_rank_image hrank_img = {};
				
				char *filename = get_hand_rank_filename((hand_rank)i);
				hrank_img.bmp = debug_load_bitmap(filename);
				
				global_assets.hrank_images[i] = hrank_img;
			}
			
			POINT pnt;
			pnt.x = 500;
			pnt.y = 250;
			
			POINT pnt_arr[1];
			pnt_arr[0] = pnt;
			
			int dp_result = DPtoLP(device_context, pnt_arr, 1);
			
			G_STATE.text_pos.x = pnt_arr[0].x;
			G_STATE.text_pos.y = pnt_arr[0].y;
			
			for (int i = 0; i < 52; i++) {
				char path[50];
				get_filename_for_card(path, G_STATE.deck.cards[i]);
				bitmap_result bmp = debug_load_bitmap(path);
				
				card_image c_image = {};
				c_image.bmp = bmp;
				strcpy(c_image.path, (const char *)path);
				
				OutputDebugStringA("c_image path:\n");
				OutputDebugStringA(c_image.path);
				
				global_assets.card_images[i] = c_image;
			}
			
			// MESSAGE LOOP
			while (!should_quit) {
				MSG msg;
				
				while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) {
						should_quit = true;
					}
					
					TranslateMessage(&msg);
					DispatchMessageA(&msg);
				}
				
				debug_paint_window(0xA83232);
				// debug_render_bitmap(bmp);

				update_and_render(&G_STATE, &global_buffer, &global_assets, device_context);
				
				window_dimension dimension = get_window_dimension(window_handle);
				display_buffer_in_window(device_context, dimension);
			}
		} else {
			OutputDebugStringA("ERROR: Unable to create window.");
		}
	} else {
		OutputDebugStringA("ERROR: Unable to register the window class.");
	}
}