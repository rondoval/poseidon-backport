/* Catalog layout version, passed to OpenCatalog() as OC_Version (locale.c). A .ct must
 * declare the same major in its `## version $VER: Trident.catalog <maj>.<min>` line or
 * locale.library refuses it and Trident falls back to the built-in English.
 *
 * Bump whenever trident.cd gains, loses or reorders a message: the MSG_* number is both
 * the catalog string id AND the index into CatCompArray (see locale.c), so any change to
 * the string list renumbers everything after it and makes older catalogs return the wrong
 * text. Rejecting them is the point.
 *
 * 2 -> 3: MSG_APP_VERSION and MSG_WINDOW_TITLE removed. Both carried a hardcoded version
 *         number, which now comes from project(VERSION) via PSD_VER/PSD_NAME_VER.
 */

#define CATALOG_VERSION  3
