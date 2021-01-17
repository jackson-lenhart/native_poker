#include <windows.h>
#include <stdio.h>

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
	char id[3];
};

struct hand_rank_image {
	bitmap_result bmp;
};

struct game_assets {
	card_image card_images[52];
	hand_rank_image hrank_images[10];
};

struct coordinate {
	int x;
	int y;
};

// GLOBALS
bool should_quit = false;

win32_offscreen_buffer global_buffer;
game_assets global_assets;

// These are to be used for for loading the .bmp files
char suit_chars[4] = { 'h', 'd', 'c', 's' };
char *value_strings[13] = { "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12", "13" };

HDC device_context;

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
			unsigned int file_size32 = (unsigned int)file_size.QuadPart;	// NOTE: Probably unsafe truncation here
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

	return bmp_result;
}

void debug_paint_window(unsigned int color) {
	unsigned int *pixel = (unsigned int *)global_buffer.memory; 
	
	for (int i = 0; i < global_buffer.height; i++) {
		for (int j = 0; j < global_buffer.width; j++) {
			*pixel++ = color;
		}
	}
}

void render_bmp(int x_pos, int y_pos, win32_offscreen_buffer *buffer, bitmap_result bmp) {
	int width = bmp.info_header->biWidth;
	int height = bmp.info_header->biHeight;

	unsigned int *dest_row = (unsigned int *)buffer->memory;
	dest_row += y_pos * (buffer->pitch / 4) + x_pos;

	// NOTE: Doing this calculation on the source row because the bitmaps are bottom up,
	// whereas the window is top-down. So must start at the bottom of the source bitmap,
	// working left to right.
	unsigned int *source_row = bmp.pixels + ((bmp.stride / 4) * (height - 1));
	
	for (int y = y_pos; y < y_pos + height; y++) {
		unsigned char *dest = (unsigned char *)dest_row;
		unsigned char *source = (unsigned char *)source_row;
		
		for (int x = x_pos; x < x_pos + width; x++) {
			for (int i = 0; i < 3; i++) {
				*dest = *source;
				dest++;
				source++;
			}
			
			*dest = 0xFF;
			dest++;
		}

		dest_row += buffer->pitch / 4;
		source_row -= bmp.stride / 4;
	}
}

#include "poker.cpp"

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
				reset_hand();
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
			
			// create_deck(G_STATE.deck.cards);
			
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
			
			int asset_index = 0;
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 13; j++) {
					char path[50];
					sprintf(path, "c:/projects/native_poker/card-BMPs/%c%s.bmp", suit_chars[i], value_strings[j]);
					bitmap_result bmp = debug_load_bitmap(path);
					
					card_image c_image = {};
					c_image.bmp = bmp;
					sprintf(c_image.id, "%c%s", suit_chars[i], value_strings[j]);
					
					global_assets.card_images[asset_index] = c_image;
					asset_index++;
				}
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

				update_and_render(&global_buffer, &global_assets);
				
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