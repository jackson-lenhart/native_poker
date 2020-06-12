#include <windows.h>
#include <stdint.h>

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

#include "poker.cpp"

win32_offscreen_buffer global_buffer;
game_assets global_assets;
bool should_quit = false;

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