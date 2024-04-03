<?php

/**
 * @file
 * Filter hook implementations and callbacks.
 */

/**
 * Implements hook_filter_info().
 */
function entity_embed_filter_info() {
  $filters['entity_embed'] = array(
    'title' => t('Display embedded entities'),
    'description' => t('Embeds entities using data attributes: data-entity-type, data-entity-id, and data-view-mode. Should usually run as the last filter, since it does not contain user input.'),
    'type' => TYPE_TRANSFORM_REVERSIBLE,
    'process callback' => '_entity_embed_render_placeholders',
    'cache' => FALSE,
    'tips callback' => '_entity_embed_render_placeholders_tips',
    'weight' => 100,
  );

  return $filters;
}

/**
 * Implements callback_filter_tips().
 *
 * Provides help for the entity embed's render placeholder filter.
 */
function _entity_embed_render_placeholders_tips($filter, $format, $long = FALSE) {
  if ($long) {
    return t('
        <p>You can embed entities. Additional properties can be added to the embed tag like data-caption and data-align if supported. Examples:</p>
        <ul>
          <li>Embed by ID: <code>&lt;drupal-entity data-entity-type="node" data-entity-id="1" data-view-mode="teaser" /&gt;</code></li>
          <li>Embed by UUID: <code>&lt;drupal-entity data-entity-type="node" data-entity-uuid="07bf3a2e-1941-4a44-9b02-2d1d7a41ec0e" data-view-mode="teaser" /&gt;</code></li>
        </ul>');
  }
  else {
    return t('You can embed entities.');
  }
}

/**
 * Implements callback_filter_process().
 *
 * Converts embedded entity placeholders into rendered entities.
 */
function _entity_embed_render_placeholders($text, $filter, $format, $langcode, $cache, $cache_id) {
  $result = $text;

  if (strpos($text, 'data-entity-type') !== FALSE && (strpos($text, 'data-entity-embed-display') !== FALSE || strpos($text, 'data-view-mode') !== FALSE)) {
    $dom = entity_embed_dom_load_html($text);
    $xpath = new \DOMXPath($dom);

    foreach ($xpath->query('//drupal-entity[@data-entity-type and (@data-entity-uuid or @data-entity-id) and (@data-entity-embed-display or @data-view-mode)]') as $node) {
      $entity_type = $node->getAttribute('data-entity-type');
      $entity = NULL;
      $entity_output = '';

      // data-entity-embed-settings is deprecated, make sure we convert it to
      // data-entity-embed-display-settings.
      if (($settings = $node->getAttribute('data-entity-embed-settings')) && !$node->hasAttribute('data-entity-embed-display-settings')) {
        $node->setAttribute('data-entity-embed-display-settings', $settings);
        $node->removeAttribute('data-entity-embed-settings');
      }

      try {
        // Load the entity either by UUID (preferred) or ID.
        $uuid = $node->getAttribute('data-entity-uuid');
        $id = $node->getAttribute('data-entity-id');

        if (!empty($uuid) && module_exists('uuid')) {
          $entity = entity_uuid_load($entity_type, array($uuid));
          $entity = reset($entity);
        }
        else {
          $entity = entity_load_single($entity_type, $id);
        }

        if ($entity) {
          // Protect ourselves from recursive rendering.
          static $depth = 0;
          $depth++;
          if ($depth > 20) {
            throw new EntityEmbedRecursiveRenderingException(sprintf('Recursive rendering detected when rendering embedded %s entity %s.', $entity_type, $id));
          }

          // If a UUID was not used, but is available, add it to the HTML.
          if (!$node->getAttribute('data-entity-uuid') && !empty($entity->uuid)) {
            $node->setAttribute('data-entity-uuid', $entity->uuid);
          }

          $context = entity_embed_get_dom_node_attributes_as_array($node);
          $context += array('data-langcode' => $langcode);
          $entity_output = entity_embed_render_entity($entity_type, $entity, $context);

          $depth--;
        }
        else {
          throw new EntityEmbedEntityNotFoundException(sprintf('Unable to load embedded %s entity %s.', $entity_type, $id));
        }
      }

      catch(\Exception $e) {
        watchdog_exception('entity_embed', $e);
      }

      entity_embed_replace_dom_node_content($node, $entity_output);
    }

    $result = entity_embed_serialize($dom);
  }

  return $result;
}
