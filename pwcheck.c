/*******************************************************************************
 *
 *	pwcheck.c - vypracování projektu v rámci IZP na FIT VUT
 *	Copyright (C) 2021 Prokop Schield
 *
 *	Tento program je svobodný software: lze jej upravovat anebo šířit
 *	pod podmínkami, které upravuje licence GNU GPL, ve znění publikace
 *	nadace FSF, a to verze 3 této licence, nebo verze pozdější.
 *
 *	Text licence je dostupný zde: <https://www.gnu.org/licenses/gpl-3.0>
 *
\******************************************************************************/

#include <stdio.h>

/*********************/
/*                   */
/*    Konfigurace    */
/*                   */
/*********************/

/** Délka heslového pole **/
#define password_field_length 256
// < 101 nebude splňovat zadání délky hesla 100
// <= 101 nebude házet chybu pro příliš dlouhá hesla

// Pravidlo 3:
// Zapnout ochranu proti -p 1 ?
// Zapnutí této ochrany nastaví
// pro pravidlo 3 minimální PARAM 2.
// Vypnutí této ochrany vyháže
// všechna hesla při P <= 1.

// pro vypnutí ochrany zakomentujte následující řádek:
// #define conf_test3_protect_against_p_1

// počet argumentů
#define number_of_arguments 2

// zapnout striktní ochranu proti špatným argumentům?
#define strict_argument_checking true

// vyhodit chybu, pokud je heslo příliš dlouhé?
#define strict_length_checking true
#define strict_length_limit 100

// minimální, maximální level, param
#define min_level 1
#define max_level 4
#define min_param 1
#define max_param_test2 4

/**********************************/
/*                                */
/*  Sekce konstant derivovaných   */
/*         z konfigurační části   */
/*                                */
/**********************************/

#ifdef conf_test3_protect_against_p_1
#define test3_min 2
#else
#define test3_min 1
#endif

// Limit na velikost hesla by měl být o 1 nižší než velikost pole.
const size_t password_max_length = (password_field_length - 1);

/****************************/
/*                          */
/*  Sekce obecných funkcí   */
/*             a definicí   */
/*                          */
/****************************/

typedef size_t let;
typedef unsigned char bool;
#define false 0
#define true 1

// pozovnat dva řetězce
char strings_differ(char *left, char *right) {
	char L;
	char R;
	while (
		// L = písmeno v levém řetězci
		(L = *(left++)),
		// R = písmeno v pravém řetězci
		(R = *(right++)),
		// pokračovat, pokud nejsme na konci řetězce
		(L || R)
	) {
		// Pokud se stringy liši, vrátit levé, popř. pravé písmeno.
		if (L != R)
			return L ? L : R;
	}
	return 0;
}

// pozorvnat dva podřetězce o délce n
char substr_differ(char *left, char *right, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		if (left[i] != right[i])
			return left[i] - right[i];
	}
	return 0;
}

#define is_uc_letter(c) ((c >= 'A') && (c <= 'Z'))
#define is_lc_letter(c) ((c >= 'a') && (c <= 'z'))
#define is_letter(c) (is_lc_letter(c) || is_uc_letter(c))
#define is_numeral(c) ((c >= '0') && (c <= '9'))

#define min(a, b) ((a < b) ? a : b)
#define max(a, b) ((a > b) ? a : b)

#define jsor(a, b) (a ? a : b)
#define jsand(a, b) (!a ? b : a)

#define isNotFalse(a) (!!(a))

#define print_mul(a, ...) fprintf(stdout, a "\n", __VA_ARGS__)
#define print_err(a, ...) fprintf(stderr, a "\n", __VA_ARGS__)

/**********************************/
/*                                */
/*  Konec sekce obecných funkcí   */
/*                                */
/**********************************/

#define nchars_length 256

typedef struct {
	unsigned char argument_mode;
	long long nezarazene[number_of_arguments];
	unsigned char nezarazene_index;
} ArgReaderState;

typedef struct {
	bool nchar[nchars_length];
	unsigned short int minlen;
	unsigned long long counter;
	double length_sum;
} StatsState;

typedef struct {
	unsigned short int LEVEL;
	unsigned long long PARAM;
	unsigned short int STATS;
	ArgReaderState argr;
	StatsState stats;
} State;

// Chybové kódy - vraceny (nejen) funkcí main
#define status_success 0
#define status_failed 1
#define status_invalid_input 2

/***********************************/
/*                                 */
/*  Sekce zabývající se argumenty  */
/*                                 */
/***********************************/

#define arg_reading_flag (0x01)
#define arg_reading_level (0x02)
#define arg_reading_param (0x04)
#define arg_level_defined (0x08)
#define arg_param_defined (0x10)
#define arg_stats_defined (0x20)

#define arg_flag_mask (0x3F)

#define set_flag(a) (state->argr.argument_mode |= a)
#define rem_flag(a) (state->argr.argument_mode &= (arg_flag_mask ^ a))
#define has_flag(a) (!!(state->argr.argument_mode & a))


// funkce na zařazení argumentu později
int zaradit_argument_pozdeji(State* state, long long n) {
	// pokud již máme dva číselné argumenty, je nelogické přijmout další;
	// vracíme chybu: příliš mnoho argumentů
	if (state->argr.nezarazene_index < number_of_arguments) {
		state->argr.nezarazene[state->argr.nezarazene_index++] = n;
		return status_success;
	} else {
		return status_invalid_input;
	}
}

// Sežereme argumenty. Chceme prémiové body.
int handle_argument(State *state, char *arg) {
	// Zkusme zjistit, zda nám uživatel věnoval číslo.
	long long n = 0;
	size_t did_read = sscanf(arg, "%lld", &n);
	if (did_read) {
		// Takže nám uživatel dal číslo! Super :)
		if (has_flag(arg_reading_flag)) {
			// Na číslo jsme dokonce čekali!
			if (has_flag(arg_reading_level)) {
				// Dostali jsme vlaječku -l LEVEL, nastavíme proměnnou :)
				state->LEVEL = (unsigned short int)n;
				// Nezapomeňme nastavit, že další číslo již nečekáme.
				rem_flag((arg_reading_level | arg_reading_flag));
				// LEVEL úspěšně nastaven.
				set_flag(arg_level_defined);
			} else if (has_flag(arg_reading_param)) {
				// Dostali jsme vlaječku -p PARAM, nastavíme proměnnou :)
				state->PARAM = (unsigned long long)n;
				// Nezapomeňme nastavit, že další číslo již nečekáme.
				rem_flag((arg_reading_param | arg_reading_flag));
				// LEVEL úspěšně nastaven.
				set_flag(arg_param_defined);
			}
			// ELSE: pokud by se přidaly další vlaječky, přišly by sem.
		} else {
			// Striktně hodit chybu, pokud dostaneme argument <= 0
			if (strict_argument_checking) {
				if (n <= 0) {
					return status_invalid_input;
				}
			}
			// Zařadit argument; pokud je argumentů příliš, nahlásit chybný vstup.
			if (zaradit_argument_pozdeji(state, n) && strict_argument_checking) {
				return status_invalid_input;
			};
		}
	} else if (!strings_differ("-l", arg)) {
		// očekávat level flag
		rem_flag(arg_reading_param);
		set_flag((arg_reading_level | arg_reading_flag));
	} else if (!strings_differ("-p", arg)) {
		// očekávat param flag
		rem_flag(arg_reading_level);
		set_flag((arg_reading_param | arg_reading_flag));
	} else if (!strings_differ("--stats", arg)) {
		state->STATS = true;
		set_flag(arg_stats_defined);
	} else {
		return status_invalid_input;
	}
	return status_success;
}

// Zbytek nezařazených parametrů
void remaining_arguments(State *state) {
	for (size_t i = 0; i < state->argr.nezarazene_index; ++i) {
		long long n = state->argr.nezarazene[i];
		if (n) {
			if (!(state->argr.argument_mode & arg_level_defined)) {
				state->LEVEL = n;
				state->argr.argument_mode |= arg_level_defined;
			} else if (!(state->argr.argument_mode & arg_param_defined)) {
				state->PARAM = n;
				state->argr.argument_mode |= arg_param_defined;
			} else
				break;
		}
	}
}

// Hlavní funkce řešící parametry
int process_arguments(State *state, int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		if (handle_argument(state, argv[i])) {
			return i;
		};
	}
	remaining_arguments(state);
	return status_success;
}

/*****************************************/
/*                                       */
/*  Konec sekce zabývající se argumenty  */
/*                                       */
/*****************************************/

// přidat heslo do statistik
void add_to_stats(State *state, size_t len, char *heslo) {
	state->stats.minlen = min(len, state->stats.minlen);
	++state->stats.counter;
	state->stats.length_sum += len;
	for (size_t i = 0; i < len; ++i) {
		state->stats.nchar[(unsigned char)heslo[i]] = 1;
	}
}

// vytisknout statistiky
void print_stats(State *state) {
	puts("Statistika:");
	short int nchars = 0;
	for (short int i = 0; i < nchars_length; ++i) {
		if (state->stats.nchar[i])
			++nchars;
	}
	print_mul("Ruznych znaku: %d", nchars);
	print_mul("Minimalni delka: %d", nchars ? (state->stats.minlen) : 0);
	print_mul(
		"Prumerna delka: %.*lf",
		1,
		nchars ? (state->stats.length_sum / state->stats.counter) : 0
	);
}

#define test2_male 1
#define test2_velke 2
#define test2_cislo 4
#define test2_symbol 8
#define test2_all 15

// hlavní testovací funkce
int test_password(State *state, size_t len, char *heslo) {
	if (state->LEVEL >= 1) {
		bool flag_uc = 0;
		bool flag_lc = 0;
		for (let i = 0; i < len; ++i) {
			if (
				(
					(flag_uc || (is_uc_letter(heslo[i]) && (flag_uc = true))) +
					(flag_lc || (is_lc_letter(heslo[i]) && (flag_lc = true)))
				) == 2
			) break;
		}
		if (!flag_uc || !flag_lc)
			return status_failed;
	}
	if (state->LEVEL >= 2) {
		unsigned char flag = 0;
		// limit: podmínka, kdy cyklus ukončit dříve
		unsigned char lim =
				min((1 << max(max_param_test2, state->PARAM)) - 1, test2_all);
		for (let i = 0; (flag != lim) && (i < len); ++i) {
			char c = heslo[i];
			if (is_uc_letter(c)) {
				flag |= test2_velke;
			} else if (is_lc_letter(c)) {
				flag |= test2_male;
			} else if (is_numeral(c)) {
				flag |= test2_cislo;
			} else {
				flag |= test2_symbol;
			}
		}

		// Pokud jsou splněny všechny podmínky, nebo je počet splněných podmínek <=
		// PARAM:
		//	pak pokračovat
		// pokud ne:
		//	vrátit status_failed
		if (!(
			(
				// splněny vsechny podminky
				flag == lim
			) || (
				(
					isNotFalse(flag & test2_male) +
					isNotFalse(flag & test2_velke) +
					isNotFalse(flag & test2_cislo) +
					isNotFalse(flag & test2_symbol)
				) >= (int)state->PARAM
			)
		)) {
			return status_failed;
		}
	}
	if (state->LEVEL >= 3) {
		for (let i = 0; i < len;) {
			register char c = heslo[i];
			bool f = 0;
			for (let j = i + 1, l = i + max(state->PARAM, test3_min); j < l; ++j) {
				if (heslo[j] != c) {
					f = i = j;
					break;
				}
			}
			if (!f)
				return status_failed;
		}
	}
	if (state->LEVEL >= 4) {
		size_t l = len - state->PARAM + 1;
		for (size_t i = 0; i < l; ++i) {
			for (size_t j = i + 1; j < l; ++j) {
				if (!substr_differ(heslo + i, heslo + j, state->PARAM))
					return status_failed;
			}
		}
	}
	return status_success;
}

// Funkce načítající heslo do paměti.
// Vrací délku načteného hesla.
int read_next_password(char *heslo) {
	// do proměnné c budeme dávat jednotlivé načtené znaky
	int c = 0;
	// do proměnné length budeme dávat průběžnou délku hesla
	size_t length = 0;
	while (!length) {
		while (
				// Načíst další znak jen pokud není heslo příliš dlouhé!
				(length < password_max_length)
				// NULL považujme za konec hesla
				&& (c = getchar())
				// EOF považujme za konec hesla
				&& (c != EOF)
				// EOL považujme za konec hesla
				&& (c != '\n')
				// CR považujme za konec hesla
				&& (c != '\r')
			) {
			// znak načteme do pole
			heslo[length++] = c;
		}
		// heslo ukončíme znakem NULL
		heslo[length] = 0;

		if (!length) {
			// Pokud jsme našli prázdný řádek, vracíme délku 0
			if (c == '\n') {
				return 0;
			// Pokud jsme našli EOF, vracíme EOF (konec)
			} else if (c == EOF) {
				return EOF;
			}
		}
	}
	// vrátíme délku hesla
	return length;
}

// Hlavní funkce. Tady začíná veškerá zábava.
int main(int argc, char *argv[]) {

	// Inicializujeme stav programu
	State state = {1, 1, 0, {0, {0}, 0}, {{0}, nchars_length, 0, 0}};

	// Spracujeme argumenty; pokud by byl některý argument neplatný,
	// jeho index dostaneme zpátky.
	int bad_argument = process_arguments(&state, argc, argv);
	// Pokud by byl některý argument neplatný, vypíšeme
	// chybovou hlášku a program ukončíme.
	if (bad_argument) {
		print_err(
			"Input error: invalid argument %d: \"%s\"\n",
			bad_argument,
			argv[bad_argument]
		);
		// Ukončíme program s chybovým kódem
		return status_invalid_input;
	}
	if (strict_argument_checking) {
		if ((state.LEVEL < min_level) || (state.LEVEL > max_level)) {
			print_err(
				"Level must be between %d and %d; received %d",
				min_level,
				max_level,
				state.LEVEL
			);
			return status_invalid_input;
		}
		if (state.PARAM < min_param) {
			print_err(
				"Param must be greater than %d; received %lld",
				min_param,
				state.PARAM
			);
			return status_invalid_input;
		}
	}

	// Vytvoříme heslové pole o velikosti,
	// kterou definuje konfigurace
	char heslo[password_field_length] = {0};
	int len;
	while ((len = read_next_password(heslo)) != EOF) {
		if (strict_length_checking && (len > strict_length_limit)) {
			print_err(
				"The password '%s' is too long.",
				heslo
			);
			return status_invalid_input;
		}
		if (!test_password(&state, len, heslo)) {
			puts(heslo);
		}
		if (state.STATS) {
			add_to_stats(&state, len, heslo);
		}
	}
	if (state.STATS)
		print_stats(&state);
	return status_success;
}
