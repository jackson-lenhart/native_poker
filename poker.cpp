#include <time.h>

struct card {
	int value;
	char suit;
	bool removed_from_deck;
};
/*
struct hand {
	card cards[5];
	unsigned char folded;  // BOOL
};
*/
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

struct player {
	int stack;
	card hand[2];
	bool folded;
};

struct game_state {
	player players[2];
	card board[5];
	deck_info deck;
//	coordinate text_pos;
	hand_status h_status;
	hand_rank ranks[2];
	int flags;
	char bet_input[32];
	int current_big_blind_index;
	int pot;
	int action_index;
	int winner_index;
};

// FLAGS
int HAND_INITIALIZED = 1 << 0;
int DECK_CREATED = 1 << 1;
int GAME_INITIALIZED = 1 << 2;

// GLOBALS
char *suits = "hdcs";
int sorted_wheel_values[5] = { 2, 3, 4, 5, 14 };
// int proper_wheel_values[5] = { 1, 2, 3, 4, 5  };

game_state G_STATE = {};

void swap_cards(card *card1, card *card2) {
	card tmp = *card1;
	*card1 = *card2;
	*card2 = tmp;
}

void sort_combined_hand(card *combined_hand) {
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6 - i; j++) {
			if (combined_hand[j].value > combined_hand[j + 1].value) {
				swap_cards(&combined_hand[j], &combined_hand[j + 1]);
			}
		}
	}
}

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

void hand_generator(card *hand, card *deck) {
	hand[0] = extract_random_card(deck);
	hand[1] = extract_random_card(deck);
}
/*
void debug_print_hand(card *hand) {
	for (int i = 0; i < 5; i++) {
		char buffer[3];
		sprintf(buffer, "%d%c", hand[i].value, hand[i].suit);
		OutputDebugStringA(buffer);
	}
	OutputDebugStringA("\n");
}
*/
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
void aces_high_unless_wheel(card *hand) {
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
			return "INVALID";
	}
}

// Assumes sorted hand
hand_rank evaluate_hand(card *hand) {
	// sort_hand(hand);
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

// Assumes hands are 5 sorted cards  (TODO: Assert)
// Returns 1 if hand1 wins, 2 if hand2 wins, and 0 if tie.
int resolve_tie(card *hand1, card *hand2, hand_rank rank) {
	if (rank == Straight || rank == StraightFlush) {
		if (hand1[4].value > hand2[4].value) {
			return 1;
		} else if (hand1[4].value < hand2[4].value) {
			return 2;
		} else {
			return 0;
		}
	}

	if (rank == Flush) {
		for (int i = 4; i >= 0; i--) {
			if (hand1[i].value > hand2[i].value) {
				return 1;
			} else if (hand1[i].value < hand2[i].value) {
				return 2;
			}
		}

		return 0;
	}

	if (rank == Quads) {
		card quad_card1 = hand1[0];
		card kicker1 = hand1[4];
		if (hand1[1].value != quad_card1.value) {
			quad_card1 = hand1[1];
			kicker1 = hand1[0];
		}

		card quad_card2 = hand2[0];
		card kicker2 = hand2[4];
		if (hand2[1].value != quad_card2.value) {
			quad_card2 = hand2[1];
			kicker2 = hand2[0];
		}

		if (quad_card1.value > quad_card2.value) {
			return 1;
		} else if (quad_card1.value < quad_card2.value) {
			return 2;
		} else {
			if (kicker1.value > kicker2.value) {
				return 1;
			} else if (kicker1.value < kicker2.value) {
				return 2;
			} else {
				return 0;
			}
		}
	}
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

bool is_bet_input_empty(char *bet_input) {
	int i = 0;
	while (bet_input[i] != 0) i++;
	if (i > 0) return false;
	else return true;
}

void clear_bet_input() {
	for (int i = 0; i < 32; i++) {
		G_STATE.bet_input[i] = 0;
	}
}

void increment_h_status() {
	int x = (int)G_STATE.h_status;
	x++;
	G_STATE.h_status = (hand_status)x;
}

void get_card_image_id(card c, char *id_buffer) {
	char stringified_value[3];

	if (c.value < 10) {
		sprintf(stringified_value, "0%d", c.value);
	} else if (c.value == 14) {
		sprintf(stringified_value, "0%d", 1);
	} else {
		sprintf(stringified_value, "%d", c.value);
	}

	sprintf(id_buffer, "%c%s", c.suit, stringified_value);
}

int get_card_asset_index(char *id) {
	for (int i = 0; i < 52; i++) {
		if (strcmp(global_assets.card_images[i].id, id) == 0) {
			return i;
		}
	}

	return 0;
}

struct best_possible_hand_result {
	card *hand_ptr;
	hand_rank rank;
};

// Expects sorted combined hand
best_possible_hand_result get_best_possible_hand(card *combined_hand) {
	const int NUM_POSSIBLE_HANDS = 21;	// Total subsets of 5 possible in 7 cards (hand + board)
	card possible_hands[5 * NUM_POSSIBLE_HANDS];

	int h = 0;
	while (h < 5 * NUM_POSSIBLE_HANDS) {
		for (int i = 0; i < 3; i++) {
			for (int j = i + 1; j < 4; j++) {
				for (int k = j + 1; k < 5; k++) {
					for (int l = k + 1; l < 6; l++) {
						for (int m = l + 1; m < 7; m++) {
							possible_hands[h]     = combined_hand[i];
							possible_hands[h + 1] = combined_hand[j];
							possible_hands[h + 2] = combined_hand[k];
							possible_hands[h + 3] = combined_hand[l];
							possible_hands[h + 4] = combined_hand[m];

							h += 5;
						}
					}
				}
			}
		}
	}

	hand_rank best_hand_rank = evaluate_hand(possible_hands);
	card *best_hand_ptr = possible_hands;
	int i = 1;
	card *curr = possible_hands + 5;
	while (i < NUM_POSSIBLE_HANDS) {
		hand_rank curr_hand_rank = evaluate_hand(curr);
		if (curr_hand_rank < best_hand_rank) {
			best_hand_rank = curr_hand_rank;
			best_hand_ptr = curr;
		}

		curr += 5;
		i++;
	}

	best_possible_hand_result result;
	result.hand_ptr = best_hand_ptr;
	result.rank = best_hand_rank;

	return result;
}

void update_and_render(win32_offscreen_buffer *buffer, game_assets *assets, keyboard_input *k_input) {
	if (G_STATE.h_status < Showdown) {
		if (k_input->digit_pressed > -1) {
			char digit_str[2];
			itoa(k_input->digit_pressed, digit_str, 10);

			int i = 0;
			while (G_STATE.bet_input[i] != 0 && i < 32) {
				i++;
			}

			if (i < 32) {	// Do nothing if we iterated all the way through as that means the buffer is filled with chars.
				G_STATE.bet_input[i] = digit_str[0];
			}
		} else if (k_input->backspace_pressed) {
			int i = 0;
			while (G_STATE.bet_input[i] != 0 && i < 32) {
				i++;
			}

			if (i > 0) {	// Do nothing if bet_input is empty
				G_STATE.bet_input[i - 1] = 0;
			}
		}
	}

	if (k_input->return_pressed) {
		if (G_STATE.h_status == Showdown) {
			G_STATE.flags &= ~HAND_INITIALIZED;
			G_STATE.current_big_blind_index = 1 - G_STATE.current_big_blind_index;
		} else if (G_STATE.players[0].stack <= 0 || G_STATE.players[1].stack <= 0) {
			increment_h_status();
		} else if (!is_bet_input_empty(G_STATE.bet_input)) {
			int bet_amount = atoi(G_STATE.bet_input);
			if (G_STATE.players[0].stack >= bet_amount) {
				G_STATE.pot += bet_amount;
				G_STATE.players[0].stack -= bet_amount;

				clear_bet_input();

				// TODO: AI response. For now CPU just calls every time...
				G_STATE.pot += bet_amount;
				G_STATE.players[1].stack -= bet_amount;

				if (G_STATE.h_status == PreFlop) {
					G_STATE.board[0] = extract_random_card(G_STATE.deck.cards);
					G_STATE.board[1] = extract_random_card(G_STATE.deck.cards);
					G_STATE.board[2] = extract_random_card(G_STATE.deck.cards);
				} else if (G_STATE.h_status == Flop) {
					G_STATE.board[3] = extract_random_card(G_STATE.deck.cards);
				} else if (G_STATE.h_status == Turn) {
					G_STATE.board[4] = extract_random_card(G_STATE.deck.cards);
				} else if (G_STATE.h_status == River) {
					// TODO: Find out actual 5 card hand to use for each player given the player's 2 card hand
					// plus the board (all 5 card permutations of 7 cards) and solve issue with sorting (make a copy... figure
					// out what the function should return).

					card combined_hand1[7];
					combined_hand1[0] = G_STATE.players[0].hand[0];
					combined_hand1[1] = G_STATE.players[0].hand[1];

					for (int i = 2; i < 7; i++) {
						combined_hand1[i] = G_STATE.board[i - 2];
					}

					card combined_hand2[7];
					combined_hand2[0] = G_STATE.players[1].hand[0];
					combined_hand2[1] = G_STATE.players[1].hand[1];

					for (int i = 2; i < 7; i++) {
						combined_hand2[i] = G_STATE.board[i - 2];
					}

					sort_combined_hand(combined_hand1);
					sort_combined_hand(combined_hand2);

					best_possible_hand_result best_hand_result1 = get_best_possible_hand(combined_hand1);
					best_possible_hand_result best_hand_result2 = get_best_possible_hand(combined_hand2);

					if (best_hand_result1.rank < best_hand_result2.rank) {
						G_STATE.players[0].stack += G_STATE.pot;
						G_STATE.winner_index = 0;
					} else if (best_hand_result2.rank < best_hand_result1.rank) {
						G_STATE.players[1].stack += G_STATE.pot;
						G_STATE.winner_index = 1;
					} else {
						G_STATE.winner_index = -1;	// Tie

						G_STATE.players[0].stack += G_STATE.pot / 2;
						G_STATE.players[1].stack += G_STATE.pot / 2;
						// @Fix: create round robin for "odd chip"
					}

					/*
					G_STATE.ranks[0] = evaluate_hand(G_STATE.players[0].hand);
					G_STATE.ranks[1] = evaluate_hand(G_STATE.players[1].hand);

					if (G_STATE.ranks[0] < G_STATE.ranks[1]) {
						G_STATE.players[0].stack += G_STATE.pot;
						G_STATE.winner_index = 0;
					} else if (G_STATE.ranks[1] < G_STATE.ranks[0]) {
						G_STATE.players[1].stack += G_STATE.pot;
						G_STATE.winner_index = 1;
					} else {
						G_STATE.winner_index = -1;	// Tie
					}
					*/
				}

				increment_h_status();
			}
		}
	}

	if (!(G_STATE.flags & DECK_CREATED)) {
		create_deck(G_STATE.deck.cards);
		G_STATE.flags |= DECK_CREATED;
	}

	if (test_deck_empty(G_STATE.deck.cards)) {
		create_deck(G_STATE.deck.cards);
	}

	if (!(G_STATE.flags & GAME_INITIALIZED)) {
		G_STATE.players[0].stack = 10000;
		G_STATE.players[1].stack = 10000;

		G_STATE.flags |= GAME_INITIALIZED;
	}

	if (!(G_STATE.flags & HAND_INITIALIZED)) {
		srand(time(0));

		G_STATE.h_status = PreFlop;
		G_STATE.pot = 150;
		G_STATE.players[0].stack -= 50;
		G_STATE.players[1].stack -= 100;	// CPU is just always big blind for now.

		hand_generator(G_STATE.players[0].hand, G_STATE.deck.cards);
		hand_generator(G_STATE.players[1].hand, G_STATE.deck.cards);

		G_STATE.flags |= HAND_INITIALIZED;
	}

	for (int i = 0; i < 2; i++) {
		char id[3];
		get_card_image_id(G_STATE.players[0].hand[i], id);
		int asset_index = get_card_asset_index(id);

		render_bitmap(71 * i, 0, buffer, global_assets.card_images[asset_index].bmp);
	}

	if (G_STATE.h_status < Showdown) {
		render_bitmap(700, 0, buffer, global_assets.face_down_card_image);
		render_bitmap(700 + 71, 0, buffer, global_assets.face_down_card_image);
	} else {
		for (int i = 0; i < 2; i++) {
			char id[3];
			get_card_image_id(G_STATE.players[1].hand[i], id);
			int asset_index = get_card_asset_index(id);

			render_bitmap(700 + (i * 71), 0, buffer, global_assets.card_images[asset_index].bmp);

			if (G_STATE.winner_index == 0) {
				debug_render_string(600, 300, buffer, "Player");
			} else if (G_STATE.winner_index == 1) {
				debug_render_string(600, 300, buffer, "CPU");
			} else {
				debug_render_string(600, 300, buffer, "TIE");
			}

		}
	}

	if (G_STATE.h_status >= Flop) {
		for (int i = 0; i < 3; i++) {
			char id[3];
			get_card_image_id(G_STATE.board[i], id);
			int asset_index = get_card_asset_index(id);

			render_bitmap(200 + (71 * i), 200, buffer, global_assets.card_images[asset_index].bmp);
		}

		if (G_STATE.h_status >= Turn) {
		char id[3];
			get_card_image_id(G_STATE.board[3], id);
			int asset_index = get_card_asset_index(id);

			render_bitmap(200 + (71 * 3), 200, buffer, global_assets.card_images[asset_index].bmp);

			if (G_STATE.h_status >= River) {
				get_card_image_id(G_STATE.board[4], id);
				int asset_index = get_card_asset_index(id);

				render_bitmap(200 + (71 * 4), 200, buffer, global_assets.card_images[asset_index].bmp);
			}
		}
	}



	char stack_str1[16];
	itoa(G_STATE.players[0].stack, stack_str1, 10);
	debug_render_string(200, 50, buffer, stack_str1);

	char stack_str2[16];
	itoa(G_STATE.players[1].stack, stack_str2, 10);
	debug_render_string(900, 50, buffer, stack_str2);

	char pot_str[16];
	itoa(G_STATE.pot, pot_str, 10);
	debug_render_string(500, 450, buffer, pot_str);
	debug_render_string(500, 150, buffer, G_STATE.bet_input);
}
