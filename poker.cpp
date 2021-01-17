#include <time.h>

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
//	coordinate text_pos;
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
int DECK_CREATED = 1 << 1;

// GLOBALS
game_state G_STATE = {};
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

char *get_hand_rank_filename(hand_rank rank) {
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

void reset_hand() {
	G_STATE.flags &= ~HAND_INITIALIZED;
}

void update_and_render(win32_offscreen_buffer *buffer, game_assets *assets) {
	if (!(G_STATE.flags & DECK_CREATED)) {
		create_deck(G_STATE.deck.cards);
		G_STATE.flags |= DECK_CREATED;
	}
	
	if (test_deck_empty(G_STATE.deck.cards) || !(G_STATE.flags & DECK_CREATED)) {
		create_deck(G_STATE.deck.cards);
	}
	
	if (!(G_STATE.flags & HAND_INITIALIZED)) {
		srand(time(0));
	
		test_hand_generator(G_STATE.hands[0].cards, G_STATE.deck.cards);
		test_hand_generator(G_STATE.hands[1].cards, G_STATE.deck.cards);
		
		G_STATE.ranks[0] = evaluate_hand(G_STATE.hands[0].cards);
		debug_print_hand_rank(G_STATE.ranks[0]);
		
		G_STATE.flags |= HAND_INITIALIZED;
	}
	
	int x = 0;
	int y = 0;
	for (int i = 0; i < 5; i++) {
		char stringified_value[2];
		
		if (G_STATE.hands[0].cards[i].value < 10) {
			sprintf(stringified_value, "0%d", G_STATE.hands[0].cards[i].value);
		} else if (G_STATE.hands[0].cards[i].value == 14) {
			stringified_value[0] = '0';
			stringified_value[1] = '1';
		} else {
			sprintf(stringified_value, "%d", G_STATE.hands[0].cards[i].value);
		}
		
		char id[3];
		sprintf(id, "%c%s", G_STATE.hands[0].cards[i].suit, stringified_value);
		
		for (int j = 0; j < 52; j++) {
			if (strcmp(global_assets.card_images[j].id, id) == 0) {
				render_bmp(x, y, buffer, global_assets.card_images[j].bmp);
				
				x += (global_assets.card_images[j].bmp.info_header->biWidth);
			}
		}
	}
	
	render_bmp(750, 250, buffer, global_assets.hrank_images[G_STATE.ranks[0]].bmp);
}