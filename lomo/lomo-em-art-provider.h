/* lomo-em-art-provider.h */

#ifndef __LOMO_EM_ART_PROVIDER_H__
#define __LOMO_EM_ART_PROVIDER_H__

#include <glib-object.h>
#include <lomo/lomo-player.h>

G_BEGIN_DECLS

#define LOMO_TYPE_EM_ART_PROVIDER lomo_em_art_provider_get_type()

#define LOMO_EM_ART_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LOMO_TYPE_EM_ART_PROVIDER, LomoEMArtProvider)) 
#define LOMO_EM_ART_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  LOMO_TYPE_EM_ART_PROVIDER, LomoEMArtProviderClass)) 
#define LOMO_IS_EM_ART_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LOMO_TYPE_EM_ART_PROVIDER)) 
#define LOMO_IS_EM_ART_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  LOMO_TYPE_EM_ART_PROVIDER))
#define LOMO_EM_ART_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  LOMO_TYPE_EM_ART_PROVIDER, LomoEMArtProviderClass))

typedef struct _LomoEMArtProviderPrivate LomoEMArtProviderPrivate;
typedef struct {
	/* <private> */
	GObject parent;
	LomoEMArtProviderPrivate *priv;
} LomoEMArtProvider;

typedef struct {
	/* <private> */
	GObjectClass parent_class;
} LomoEMArtProviderClass;

const gchar *lomo_em_art_provider_get_default_cover_path(void);
const gchar *lomo_em_art_provider_get_default_cover_uri (void);
const gchar *lomo_em_art_provider_get_loading_cover_path(void);
const gchar *lomo_em_art_provider_get_loading_cover_uri (void);

GType lomo_em_art_provider_get_type (void);

LomoEMArtProvider* lomo_em_art_provider_new (void);
void               lomo_em_art_provider_set_player(LomoEMArtProvider *self, LomoPlayer *lomo);

G_END_DECLS

#endif /* __LOMO_EM_ART_PROVIDER_H__ */
