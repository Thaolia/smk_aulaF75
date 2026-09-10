#include "aula_fx.h"

/*
 * Palette secondaire d'usine, CODE 0x2D6D. Recopiée octet pour octet du dump.
 *
 * Elle occupe les 384 octets qui séparent la fin de la roue de 192 teintes
 * (0x2B2A + 576 = 0x2D6A) du début de la carte des couronnes -- deux tables
 * contiguës, ce qui a servi à en fixer les bornes.
 */
static const __code uint8_t palette[AULA_FX_PALETTE_SIZE][3] = {
    {  0,  5,255}, {  0, 17,255}, {  0, 29,255}, {  0, 48,255},
    {  0, 65,255}, {  0, 85,255}, {  0,105,255}, {  0,127,255},
    {  0,137,255}, {  0,148,255}, {  0,161,255}, {  0,171,255},
    {  0,182,255}, {  0,192,255}, {  0,202,255}, {  0,211,255},
    {  0,220,255}, {  0,229,255}, {  0,237,255}, {  0,243,255},
    {  0,250,255}, {  0,255,255}, {  0,255,250}, {  0,255,246},
    {  0,255,238}, {  0,255,231}, {  0,255,223}, {  0,255,214},
    {  0,255,205}, {  0,255,195}, {  0,255,186}, {  0,255,175},
    {  0,255,165}, {  0,255,153}, {  0,255,143}, {  0,255,130},
    {  0,255,114}, {  0,255, 93}, {  0,255, 73}, {  0,255, 54},
    {  0,255, 37}, {  0,255, 21}, {  0,255,  8}, {  0,255,  0},
    { 11,255,  0}, { 22,255,  0}, { 36,255,  0}, { 52,255,  0},
    { 70,255,  0}, { 87,255,  0}, {106,255,  0}, {126,255,  0},
    {136,255,  0}, {144,255,  0}, {152,255,  0}, {159,255,  0},
    {168,255,  0}, {176,255,  0}, {183,255,  0}, {192,255,  0},
    {198,255,  0}, {206,255,  0}, {212,255,  0}, {220,255,  0},
    {225,255,  0}, {231,255,  0}, {237,255,  0}, {243,255,  0},
    {247,255,  0}, {251,255,  0}, {255,255,  0}, {255,249,  0},
    {255,242,  0}, {255,236,  0}, {255,225,  0}, {255,215,  0},
    {255,206,  0}, {255,195,  0}, {255,183,  0}, {255,170,  0},
    {255,159,  0}, {255,145,  0}, {255,132,  0}, {255,105,  0},
    {255, 68,  0}, {255, 34,  0}, {255, 10,  0}, {255,  0,  5},
    {255,  0, 17}, {255,  0, 30}, {255,  0, 50}, {255,  0, 69},
    {255,  0, 91}, {255,  0,113}, {255,  0,131}, {255,  0,142},
    {255,  0,155}, {255,  0,165}, {255,  0,177}, {255,  0,189},
    {255,  0,199}, {255,  0,209}, {255,  0,219}, {255,  0,229},
    {255,  0,236}, {255,  0,243}, {255,  0,250}, {255,  0,255},
    {250,  0,255}, {243,  0,255}, {237,  0,255}, {228,  0,255},
    {220,  0,255}, {209,  0,255}, {200,  0,255}, {189,  0,255},
    {178,  0,255}, {168,  0,255}, {158,  0,255}, {143,  0,255},
    {131,  0,255}, {111,  0,255}, { 90,  0,255}, { 69,  0,255},
    { 50,  0,255}, { 31,  0,255}, { 16,  0,255}, {  5,  0,255},
};

void aula_fx_palette(uint8_t index, uint8_t out[3])
{
    const uint8_t i = (uint8_t)(index & (uint8_t)(AULA_FX_PALETTE_SIZE - 1));

    out[0] = palette[i][0];
    out[1] = palette[i][1];
    out[2] = palette[i][2];
}

/*
 * Couronnes de l'onde concentrique, CODE 0x2959.
 *
 * Ce ne sont PAS des distances géométriques : la couronne 8 contient (2,0) et
 * (0,0), que n'importe quelle métrique placerait bien plus près du centre.
 * C'est un ordre de propagation écrit à la main, qui suit la disposition
 * physique des touches -- raison de plus pour le recopier plutôt que de le
 * recalculer.
 *
 * Quelques identifiants désignent des colonnes 15 et 16, qui n'existent pas sur
 * ce clavier : le framebuffer d'usine fait 21 colonnes de large et cette table
 * est manifestement partagée avec un modèle plus grand. L'appelant les écarte.
 */
static const __code uint8_t rings[AULA_FX_RINGS][AULA_FX_RING_SLOTS] = {
    {0x3a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x39, 0x41, 0x32, 0x42, 0x33, 0x3b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x40, 0x48, 0x31, 0x2a, 0x2b, 0x34, 0x3c, 0x44, 0x43, 0x4a, 0x49, 0xff, 0xff},
    {0x38, 0x29, 0x22, 0x23, 0x2c, 0x2d, 0x50, 0x51, 0x52, 0x4b, 0x4c, 0xff, 0xff},
    {0x30, 0x21, 0x1a, 0x1b, 0x24, 0x58, 0x59, 0x5a, 0x53, 0x54, 0x45, 0xff, 0xff},
    {0x28, 0x19, 0x12, 0x13, 0x1c, 0x60, 0x61, 0x62, 0x5b, 0x5c, 0x4d, 0x63, 0x55},
    {0x20, 0x11, 0x0a, 0x0b, 0x14, 0x15, 0x68, 0x71, 0x69, 0x6a, 0x6b, 0x64, 0x6c},
    {0x18, 0x09, 0x02, 0x03, 0x0c, 0x0d, 0x80, 0x72, 0x73, 0x81, 0x6d, 0x74, 0x65},
    {0x10, 0x00, 0x01, 0x05, 0x04, 0x82, 0x85, 0x7d, 0x7a, 0xff, 0x75, 0x7b, 0x7c},
};

uint8_t aula_fx_ring(uint8_t ring, uint8_t slot)
{
    if (ring >= AULA_FX_RINGS || slot >= AULA_FX_RING_SLOTS) {
        return 0xff;
    }
    return rings[ring][slot];
}

/*
 * Carte de présence, dérivée de CODE 0xC500 : un bit de ligne à 1 quand la
 * grille porte une touche. Six emplacements sur quatre-vingt-dix n'en ont pas.
 *
 * La table d'usine contient l'identifiant complet de chaque touche ; seul le
 * 0xFF nous intéresse, d'où la réduction à quinze octets. Sa ligne 4 permute
 * les colonnes, exactement comme la ligne 4 de la table 0x2EED -- deux relevés
 * indépendants qui se recoupent sur la même particularité de rangée.
 */
static const __code uint8_t present[AULA_FX_COLS] = {
    0x3f, 0x3e, 0x3f, 0x1f, 0x1f, 0x3f, 0x1f, 0x1f, 0x3f, 0x3f, 0x3f, 0x1f, 0x3f, 0x3f, 0x3f,
};

uint8_t aula_fx_present(uint8_t col)
{
    return (col < AULA_FX_COLS) ? present[col] : 0;
}

/*
 * Image « gaming », plan bleu de CODE 0xCAFC (les plans rouge et vert sont
 * entièrement nuls : l'image d'usine est bleue).
 *
 * Sa lecture confirme tout notre plan de matrice par une voie indépendante :
 * les positions allumées tombent exactement sur Échap, W, A, S, D et les quatre
 * flèches de layouts/default/layout.c. Aucun réglage n'aurait pu faire
 * coïncider neuf positions par hasard.
 */
static const __code uint8_t gaming[AULA_FX_COLS] = {
    0x01, 0x08, 0x0c, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x30, 0x20,
};

uint8_t aula_fx_gaming(uint8_t col)
{
    return (col < AULA_FX_COLS) ? gaming[col] : 0;
}

/*
 * Générateur pseudo-aléatoire.
 *
 * COMPORTEMENT transcrit, GÉNÉRATEUR substitué, et c'est délibéré. L'usine
 * emploie en 0xA997 un LFSR de Galois sur 32 bits (décalage à droite, masque
 * 0xCC4C4ECE, état en XRAM 0x0F67, réamorcé à 0xA5A5 s'il tombe à zéro), seize
 * tours par appel, suivi d'une division signée en 0x4D52 pour le modulo. Tout
 * cela pour produire « une colonne au hasard » ou « une teinte au hasard ».
 *
 * Reproduire le polynôme ne rendrait pas la même suite de toute façon -- il
 * faudrait aussi la même graine et le même ordre d'appel -- et coûterait de la
 * pile et du temps dans l'ISR. Un xorshift 8 bits donne la même propriété
 * observable pour huit octets de code. Même arbitrage que pour le moteur
 * réactif, où l'on garde une teinte au lieu des trois composantes d'usine.
 */
static uint8_t rand_state = 0xa5; /* seule chose reprise de la graine d'usine */

uint8_t aula_fx_rand(void)
{
    rand_state ^= (uint8_t)(rand_state << 1);
    rand_state ^= (uint8_t)(rand_state >> 1);
    rand_state ^= (uint8_t)(rand_state << 2);
    return rand_state;
}
