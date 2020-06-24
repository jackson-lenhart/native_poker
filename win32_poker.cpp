#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

struct game_assets {
	bitmap_result *bitmaps;
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

enum hand_status {
	PreFlop,
	Flop,
	Turn,
	River,
	Showdown
};

struct card {
	int value;
	char suit;
	bool removed_from_deck;
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

win32_offscreen_buffer global_buffer;
game_assets global_assets;
bool should_quit = false;

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

void debug_paint_window_red() {
	unsigned int red = 0xFF0000;
	unsigned int *pixel = (unsigned int *)global_buffer.memory; 
	
	for (int i = 0; i < global_buffer.height; i++) {
		for (int j = 0; j < global_buffer.width; j++) {
			*pixel++ = red;
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

void debug_render_bitmap(bitmap_result bmp) {
	int width = bmp.info_header->biWidth;
	int height = bmp.info_header->biHeight;

	unsigned char *dest_row = (unsigned char *)global_buffer.memory;

	// NOTE: We do this calculation on the source row because our bitmaps are bottom up,
	// whereas our window is top-down. So we must start at the bottom of the source bitmap.
	unsigned char *source_row = (unsigned char *)(bmp.pixels + ((bmp.stride / 4) * (height - 1)));

	for (int y = 0; y < height; y++) {
		unsigned int *dest = (unsigned int *)dest_row;
		unsigned int *source = (unsigned int *)source_row;

		for (int x = 0; x < width; x++) {
			*dest = *source;
			dest++;
			source++;
		}

		dest_row += global_buffer.pitch;
		source_row -= bmp.stride;
	}
}

void render_card(int x_pos, int y_pos, win32_offscreen_buffer *buffer, bitmap_result card_bmp) {
	int width = card_bmp.info_header->biWidth;
	int height = card_bmp.info_header->biHeight;

	unsigned char* dest_row = (unsigned char*)buffer->memory + (y_pos * buffer->pitch + x_pos);

	// NOTE: We do this calculation on the source row because our bitmaps are bottom up,
	// whereas our window is top-down. So we must start at the bottom of the source bitmap.
	unsigned char* source_row = (unsigned char*)(card_bmp.pixels + ((card_bmp.stride / 4) * (height - 1)));

	for (int y = y_pos; y < y_pos + height; y++) {
		unsigned int* dest = (unsigned int*)dest_row;
		unsigned int* source = (unsigned int*)source_row;

		for (int x = x_pos; x < x_pos + width; x++) {
			*dest = *source;
			dest++;
			source++;
		}

		dest_row += buffer->pitch;
		source_row -= card_bmp.stride;
	}
}

void update_and_render(win32_offscreen_buffer *buffer, game_assets *assets) {
	card deck[52];
	create_deck(deck);

	srand(time(0));

	card test_hand1[5];
	card test_hand2[5];
	test_hand_generator(test_hand1, deck);
	test_hand_generator(test_hand2, deck);
	
	int x = 0;
	int y = 0;
	for (int i = 0; i < 5; i++) {
		char path[50];
		get_filename_for_card(path, test_hand1[i]);
		
		bitmap_result bmp = debug_load_bitmap(path);
		render_card(x, y, buffer, bmp);
		
		x += bmp.info_header->biWidth;
		y += bmp.info_header->biHeight;
	}

	// int current_x_pos = 0;
	// int current_y_pos = 0;

	// Render hand1
	// for (int i = 0; i < 5; i++) {
	//
	// }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	LRESULT result = 0;

	switch (message) {
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
			OutputDebugStringA("WM_ACTIVATEAPP");
		}
		break;

		case WM_DESTROY: {
			OutputDebugStringA("WM_DESTROY");
		}
		break;

		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			
			window_dimension dimension = get_window_dimension(window);
			display_buffer_in_window(device_context, dimension);

			EndPaint(window, &paint);
		}
		break;
		
	/*
		case WM_KEYDOWN: {
			OutputDebugStringA("KEY DOWN\n");
			
			if (w_param == VK_LEFT) {
				k_input.left = true;
				// x_offset += 5;
			}
			
			if (w_param == VK_RIGHT) {
				k_input.right = true;
				// x_offset -= 5;
			}
			
			if (w_param == VK_UP) {
				k_input.up = true;
				// y_offset += 5;
			}
			
			if (w_param == VK_DOWN) {
				k_input.down = true;
				// y_offset -= 5;
			}
			
			if (w_param == VK_RETURN) {
				k_input.enter = true;
			}
			
			if (w_param == VK_ESCAPE) {
				k_input.escape = true;
			}
		}
		break;
		*/

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
		HWND window = CreateWindowExA(0, window_class.lpszClassName, "Poker",
			                          WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
									  CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

		if (window) {
			HDC device_context = GetDC(window);
			
			window_dimension dim = get_window_dimension(window);
			resize_dib_section(&global_buffer, dim.width, dim.height);
			
			/*srand(time(0));
			
			for (int i = 0; i < 10; i++) {
				card test_hand[5];
				test_hand_generator(test_hand, deck);
				hand_rank r = evaluate_hand(test_hand);
				
				debug_print_hand_rank(r);
				debug_print_hand(test_hand);
			}*/

			card deck[52];
			create_deck(deck);
			
			char path[50];
			get_filename_for_card(path, deck[31]);
			bitmap_result bmp = debug_load_bitmap(path);
			// bitmap_result bmp = debug_load_bitmap("c:/projects/native_poker/assets/structured_art.bmp");

			// load_card_bitmaps(deck);

			// char *filename = "c:/projects/native_poker/assets/continue.bmp";
			// global_assets.bitmaps = &debug_load_bitmap(filename);
			
			while (!should_quit) {
				MSG msg;
				
				while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) {
						should_quit = true;
					}
					
					TranslateMessage(&msg);
					DispatchMessageA(&msg);
				}
				
				debug_paint_window_red();
				debug_render_bitmap(bmp);

				update_and_render(&global_buffer, &global_assets);
				
				window_dimension dimension = get_window_dimension(window);
				display_buffer_in_window(device_context, dimension);
			}
		} else {
			OutputDebugStringA("ERROR: Unable to create window.");
		}
	} else {
		OutputDebugStringA("ERROR: Unable to register the window class.");
	}
/*	
	for (int i = 0; i < 10000; i++) {
		card test_hand[5];
		test_hand_generator(test_hand, deck);
		evaluate_hand(test_hand);
		
		if (test_deck_empty(deck)) {
			create_deck(deck);
		}
	}
*/
}