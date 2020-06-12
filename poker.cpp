#include <stdio.h>
#include <stdlib.h>
#include <time.h>

char *suits = "hdcs";
int sorted_wheel_values[5] = { 2, 3, 4, 5, 14 };
// int proper_wheel_values[5] = { 1, 2, 3, 4, 5  };

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

void render_card(int x, int y, win32_offscreen_buffer *buffer, bitmap_result card_bmp) {

}

void update_and_render(win32_offscreen_buffer *buffer, game_assets *assets) {
	card deck[52];
	create_deck(deck);

	srand(time(0));

	card test_hand1[5];
	card test_hand2[5];
	test_hand_generator(test_hand1, deck);
	test_hand_generator(test_hand2, deck);


}