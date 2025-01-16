#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
    return (uint16_t) uc | (uint16_t) color << 8;
}

size_t strlen(const char* str) 
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}
#define HEAP_SIZE (1024 * 1024)
#define ALIGN(size) (((size) + 7) & ~7)
static uint8_t heap[HEAP_SIZE];
static uint8_t* heap_end = heap;
typedef struct block_header {
    size_t size;
    struct block_header* next;
} block_header_t;
static block_header_t* free_list = NULL;
void heap_initialize() {
    free_list = (block_header_t*) heap;
    free_list->size = HEAP_SIZE;
    free_list->next = NULL;
}
void* malloc(size_t size) {
    size = ALIGN(size);
    block_header_t* prev = NULL;
    block_header_t* curr = free_list;
    while (curr) {
        if (curr->size >= size + sizeof(block_header_t)) {
            if (curr->size > size + sizeof(block_header_t)) {
                block_header_t* next_block = (block_header_t*) ((uint8_t*) curr + size + sizeof(block_header_t));
                next_block->size = curr->size - size - sizeof(block_header_t);
                next_block->next = curr->next;
                curr->next = next_block;
            }
            if (prev) {
                prev->next = curr->next;
            } else {
                free_list = curr->next;
            }

            curr->size = size | 1;
            return (void*) (curr + 1);
        }

        prev = curr;
        curr = curr->next;
    }
    return NULL;
}
void free(void* ptr) {
    if (!ptr) return;

    block_header_t* block = (block_header_t*) ptr - 1;
    block->size &= ~1;
    block->next = free_list;
    free_list = block;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
volatile uint16_t* vga_buffer = (uint16_t*)0xB8000;
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer, *terminal_buffer_2;

void terminal_initialize(void) 
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*) 0xB8000;
    terminal_buffer_2 = (uint16_t*) malloc(VGA_WIDTH * VGA_HEIGHT * sizeof(uint16_t));
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
            terminal_buffer_2[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_setcolor(uint8_t color) 
{
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color); 
}

void terminal_putchar(char c) 
{
    if (c == '\n'){
        terminal_column = 0;
    }else{
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            if (++terminal_row == VGA_HEIGHT)
                terminal_row = 0;
        }
    }
}

void terminal_write(const char* data, size_t size) 
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) 
{
    terminal_write(data, strlen(data));
}

uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void wait_for_input_buffer_clear() {
    while (inb(0x64) & 0x02);
}

void wait_for_output_buffer() {
    while (!(inb(0x64) & 0x01));
}

void ps2_write_command(uint8_t command) {
    wait_for_input_buffer_clear();
    outb(0x60, command);
}

uint8_t ps2_read() {
    wait_for_output_buffer();
    return inb(0x60);
}

void ps2_keyboard_init() {
    ps2_write_command(0xF4);
}

void set_colour(uint8_t colour){
    uint16_t colour_pixel = (colour << 8) | ' ';
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer_2[y * VGA_WIDTH + x] = colour_pixel; 
        }
    }
}

void set_pole(uint8_t colour, uint8_t position) {
    if (position > 30) {
        position = 30;
    }
    uint16_t colour_pixel = (colour << 8) | ' ';
    size_t paddle_height = 5;
    size_t start_row = (position - 1) * (VGA_HEIGHT - paddle_height) / 29;
    for (size_t y = 0; y < paddle_height; y++) {
        terminal_buffer_2[(start_row + y) * VGA_WIDTH] = colour_pixel; 
    }
}

void set_reverse_pole(uint8_t colour, uint8_t position) {
    if (position > 30) {
        position = 30;
    }
    uint16_t colour_pixel = (colour << 8) | ' ';
    size_t paddle_height = 5;
    size_t start_row = (position - 1) * (VGA_HEIGHT - paddle_height) / 29;
    for (size_t y = 0; y < paddle_height; y++) {
        terminal_buffer_2[(start_row + y) * VGA_WIDTH + (VGA_WIDTH - 1)] = colour_pixel; 
    }
}

void swap_buffers() {
    uint16_t* temp = terminal_buffer;
    terminal_buffer = terminal_buffer_2;
    terminal_buffer_2 = temp;
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = terminal_buffer_2[i];
    }
}
void draw_ball(uint8_t x, uint8_t y, uint8_t color) {
    uint16_t color_pixel = (color << 8) | ' ';
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        terminal_buffer_2[y * VGA_WIDTH + x] = color_pixel;
    }
}

void terminal_write_string_at(const char* data, size_t x, size_t y) 
{
    size_t i = 0;
    while (data[i] != '\0') {
        terminal_putentryat(data[i], terminal_color, x + i, y);
        i++;
    }
}
void itoa(int num, char* str, int base) {
    int i = 0;
    bool is_negative = false;
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    if (num < 0 && base == 10) {
        is_negative = true;
        num = -num;
    }
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    if (is_negative) {
        str[i++] = '-';
    }
    str[i] = '\0';
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

void print_int_at(int num, size_t x, size_t y) {
    char num_str[20];
    itoa(num, num_str, 10);

    terminal_write_string_at(num_str, x, y);
}
void draw_score(uint8_t score1, uint8_t score2) {
    terminal_write_string_at("1: ", 2, 0);
    print_int_at(score1, 7, 0);

    terminal_write_string_at("   2: ", 12, 0);
    print_int_at(score2, 18, 0);
}

void move_ball(uint8_t *ball_x, uint8_t *ball_y, int8_t *vel_x, int8_t *vel_y, uint8_t paddle1_y, uint8_t paddle2_y, uint8_t *score1, uint8_t *score2) {
    draw_ball(*ball_x, *ball_y, 0x00);
    *ball_x += *vel_x;
    *ball_y += *vel_y;
    if (*ball_y <= 1 || *ball_y >= VGA_HEIGHT - 2) {
        *vel_y = -*vel_y;
    }
    if (*ball_x <= 1 && (*ball_y >= paddle1_y && *ball_y <= paddle1_y + 4)) {
        *vel_x = -*vel_x;
    }
    if (*ball_x >= VGA_WIDTH - 2 && (*ball_y >= paddle2_y && *ball_y <= paddle2_y + 4)) {
        *vel_x = -*vel_x;
    }
    if (*ball_x <= 0) {
        (*score2)++;
        *ball_x = VGA_WIDTH / 2;
        *ball_y = VGA_HEIGHT / 2;
        *vel_x = 1;
        *vel_y = 1;
    }
    if (*ball_x >= VGA_WIDTH - 1) {
        (*score1)++;
        *ball_x = VGA_WIDTH / 2;
        *ball_y = VGA_HEIGHT / 2;
        *vel_x = -1;
        *vel_y = -1;
    }
    draw_ball(*ball_x, *ball_y, 0xFF);
}


void delay_ms(uint32_t ms) {
    uint32_t divisor = 1193180 / 1000;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    for (uint32_t i = 0; i < ms; i++) {
        while ((inb(0x61) & 0x01) == 0);
    }
}

void kernel_main() {
    terminal_initialize();
    ps2_keyboard_init();

    uint8_t ball_x = VGA_WIDTH / 2;
    uint8_t ball_y = VGA_HEIGHT / 2;
    int8_t vel_x = 1;
    int8_t vel_y = 1;
    uint8_t paddle1_y = 15, paddle2_y = 15;
    uint8_t enter_pressed = 0;
    uint8_t score1 = 0, score2 = 0;

    terminal_writestring("                          PONG OS (Press Enter to Play)\n");

    while (1) {
        if (inb(0x64) & 0x01) {
            uint8_t scancode = ps2_read();

            if (scancode == 28) {
                enter_pressed = 1;
            }
		if (score1 >= 10){
			set_colour((uint8_t)0x00);
			terminal_writestring("PLAYER 1 WINS");
			delay_ms(1000);
			break;
		}
		if (score2 >= 10){
			set_colour((uint8_t)0x00);
			terminal_writestring("PLAYER 2 WINS");
			delay_ms(1000);
			break;
		}
            if (enter_pressed == 1) {
                set_colour((uint8_t)0x00);
                set_pole((uint8_t)0xFF, paddle1_y);
                set_reverse_pole((uint8_t)0xFF, paddle2_y);
                move_ball(&ball_x, &ball_y, &vel_x, &vel_y, paddle1_y, paddle2_y, &score1, &score2);
                draw_score(score1, score2);
				if (scancode == 72 && paddle2_y > 1) {
					--paddle2_y;
				}
				if (scancode == 80) {
					++paddle2_y;
				}
				if (scancode == 17 && paddle1_y > 1) {
					--paddle1_y;
				}
				if (scancode == 31) {
					++paddle1_y;
				}
                swap_buffers();
            }
        }
    }
    asm volatile ("hlt");
}
