<?php

declare(strict_types=1);

namespace Drupal\entity_embed\Attribute;

use Drupal\Component\Plugin\Attribute\Plugin;
use Drupal\Core\StringTranslation\TranslatableMarkup;

/**
 * Defines an Entity Embed Display plugin attribute object.
 *
 * Plugin Namespace: Plugin/entity_embed/EntityEmbedDisplay.
 *
 * @see \Drupal\entity_embed\EntityEmbedDisplay\EntityEmbedDisplayBase
 * @see \Drupal\entity_embed\EntityEmbedDisplay\EntityEmbedDisplayInterface
 * @see \Drupal\entity_embed\EntityEmbedDisplay\EntityEmbedDisplayManager
 * @see plugin_api
 */
#[\Attribute(\Attribute::TARGET_CLASS)]
class EntityEmbedDisplay extends Plugin {

  /**
   * Constructs an EntityEmbedDisplay attribute.
   *
   * @param string $id
   *   The plugin ID.
   * @param \Drupal\Core\StringTranslation\TranslatableMarkup $label
   *   The human-readable name of the Entity Embed Display plugin.
   * @param bool|array $entity_types
   *   (optional) The entity types the plugin can apply to. Set to FALSE to
   *   make the plugin valid for all entity types.
   * @param bool $no_ui
   *   (optional) Hides the plugin in the UI if this is TRUE.
   * @param bool $supports_image_alt_and_title
   *   (optional) Whether the plugin supports per-embed alt and title
   *   overrides for media entities with an image source.
   * @param class-string|null $deriver
   *   (optional) The deriver class.
   */
  public function __construct(
    public readonly string $id,
    public readonly TranslatableMarkup $label,
    public readonly bool|array $entity_types = FALSE,
    public readonly bool $no_ui = FALSE,
    public readonly bool $supports_image_alt_and_title = FALSE,
    public readonly ?string $deriver = NULL,
  ) {}

}
