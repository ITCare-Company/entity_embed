<?php

/**
 * @file
 * Hooks provided by the entity_embed module.
 */

/**
 * @addtogroup hooks
 * @{
 */

/**
 * Alter the Entity Embed field formatters.
 *
 * Usually used to remove certain Entity Embed field formatters for specific
 * entities.
 *
 * @param $formatters
 *   An array of field formatters, returned from field_info_formatter_types().
 * @param $entity_type
 *   The type of entity, i.e. 'node', 'user'.
 * @param $entity
 *   The entity to be rendered. This is used to perform special
 *   checks/processing for unruly modules. NULL if no entity is provided.
 */
function hook_entity_embed_field_formatters_alter(&$formatters, $entity_type, $entity) {
  // Do nothing if no entity is provided.
  if (!isset($entity)) {
    return;
  }

  list($id, $vid, $bundle) = entity_extract_ids($entity_type, $entity);

  // For video and audio files, limit the available options to the media player.
  if ($entity_type == 'file' && in_array($bundle, array('audio', 'video'))) {
    $formatters = array_intersect_key($formatters, array_flip(array('file:jwplayer_formatter')));
  }

  // For images, use the image formatter.
  if ($entity_type == 'file' && in_array($bundle, array('image'))) {
    $formatters = array_intersect_key($formatters, array_flip(array('image:image')));
  }

  // For nodes, use the default option.
  if ($entity_type == 'node') {
    $formatters = array_intersect_key($formatters, array_flip(array('entityreference:entityreference_entity_view')));
  }
}

/**
 * Alter the placeholder context for an embedded entity.
 *
 * @param array &$context
 *   The context array.
 * @param callable &$callback
 *   The callback function to be used.
 * @param $entity
 *   The entity being rendered.
 */
function hook_entity_embed_context_alter(&$context, &$callback, $entity) {

}

/**
 * Alter the context of an particular embedded entity type before it is rendered.
 *
 * @param array &$context
 *   The context array.
 * @param $entity
 *   The entity object.
 */
function hook_ENTITY_TYPE_embed_context_alter(&$context, $entity) {
  if (isset($context['overrides']) && is_array($context['overrides'])) {
    foreach ($context['overrides'] as $key => $value) {
      $entity->key = $value;
    }
  }
}

/**
 * Alter the result of entity_view().
 *
 * This hook is called after the content has been assembled in a structured
 * array and may be used for doing processing which requires that the complete
 * block content structure has been built.
 *
 * @param array &$build
 *   A renderable array of data, as returned from entity_view().
 * @param $entity
 *   The entity object.
 * @param array $context
 *   The context array.
 */
function hook_entity_embed_alter(&$build, $entity, $context) {
  // Remove the contextual links on all entites that provide them.
  if (isset($build['#contextual_links'])) {
    unset($build['#contextual_links']);
  }
}

/**
 * Alter the results of the particular embedded entity type build array.
 *
 * @param array &$build
 *   A renderable array representing the embedded entity content.
 * @param $entity
 *   The embedded entity object.
 * @param array $context
 *   The context array.
 */
function hook_ENTITY_TYPE_embed_alter(&$build, $entity, array &$context) {
  // Remove the contextual links.
  if (isset($build['#contextual_links'])) {
    unset($build['#contextual_links']);
  }
}

/**
 * @} End of "addtogroup hooks".
 */
