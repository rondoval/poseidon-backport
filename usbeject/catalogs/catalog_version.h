/* Catalog layout version, passed to OpenCatalog() as OC_Version (locale.c). A .ct must
 * declare the same major in its `## version $VER: USBEject.catalog <maj>.<min>` line or
 * locale.library refuses it and USBEject falls back to the built-in English.
 *
 * Bump whenever USBEject.cd gains, loses or reorders a message: the MSG_* number is both
 * the catalog string id AND the index into CatCompArray (see locale.c), so any change to
 * the string list renumbers everything after it and makes older catalogs return the wrong
 * text. Rejecting them is the point.
 */

#define CATALOG_VERSION  1
