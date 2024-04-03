<?php

/**
 * @file
 * Display functions.
 */

/**
 * Renders an embedded entity.
 *
 * @param $entity_type
 *   The type of entity, i.e. 'node', 'user'.
 * @param $entity
 *   The entity to be rendered.
 * @param array $context
 *   (optional) Array of context values, corresponding to the attributes on
 *   the embed HTML tag.
 *
 * @return string
 *   The HTML of the entity rendered with the Entity Embed Display plugin.
 */
function entity_embed_render_entity($entity_type, $entity, array $context = array()) {
  // Support the deprecated view-mode data attribute.
  if (isset($context['data-view-mode']) && !isset($context['data-entity-embed-display']) && !isset($context['data-entity-embed-display-settings'])) {
    $context['data-entity-embed-display'] = 'entityreference:entityreference_entity_view';
    $context['data-entity-embed-display-settings'] = array('view_mode' => &$context['data-view-mode']);
  }

  list($id, $vid, $bundle) = entity_extract_ids($entity_type, $entity);

  // Merge in default attributes.
  $context += array(
    'data-entity-id' => $id,
    'data-entity-type' => $entity_type,
    'data-entity-embed-display' => 'entityreference:entityreference_entity_view',
    'data-entity-embed-display-settings' => array(),
  );

  // If the data-entity-embed-display-settings isn't an array reset  it,
  // otherwise we'll encounter a fatal error when calling
  // render_entity_embed_display_plugin() further down the line.
  if (!is_array($context['data-entity-embed-display-settings'])) {
    watchdog('entity_embed', 'Invalid display settings encountered. Could not process following settings for entity type "@entity_type" with the uuid "@id": @settings', array(
      '@settings' => $context['data-entity-embed-display-settings'],
      '@entity_type' => $entity_type,
      '@id' => $id,
    ), WATCHDOG_WARNING);
    $context['data-entity-embed-display-settings'] = array();
  }

  // The default Entity Embed Display plugin has been deprecated by the
  // rendered entity field formatter.
  if ($context['data-entity-embed-display'] === 'default') {
    $context['data-entity-embed-display'] = 'entityreference:entityreference_entity_view';
  }

  // The caption text is double-encoded, so decode it here.
  if (isset($context['data-caption'])) {
    $context['data-caption'] = decode_entities($context['data-caption']);
  }

  // Allow modules to alter the entity prior to embed rendering.
  drupal_alter(array("{$context['data-entity-type']}_embed_context", 'entity_embed_context'), $context, $entity);

  // Build and render the Entity Embed Display plugin, allowing modules to
  // alter the result before rendering.
  $build = render_entity_embed_display_plugin(
    $entity_type,
    $entity,
    $context['data-entity-embed-display'],
    $context['data-entity-embed-display-settings'],
    $context
  );

  if (isset($context['class'])) {
    if (is_string($context['class'])) {
      $context['class'] = explode(' ', $context['class']);
    }
  }
  else {
    $context['class'] = array();
  }
  $context['class'][] = 'embedded-entity';

  // Maintain data- attributes.
  if (isset($context)) {
    $build['#attributes'] = $context;
  }

  // Make sure that access to the entity is respected.
  $build['#access'] = entity_access('view', $entity_type, $entity);

  // @todo Should this hook get invoked if $build is an empty array?
  drupal_alter(array("{$context['data-entity-type']}_embed", 'entity_embed'), $build, $entity, $context);
  $entity_output = drupal_render($build);

  return $entity_output;
}

/**
 * Renders an entity using an Entity Embed Display plugin.
 *
 * @param $entity_type
 *   The type of entity, i.e. 'node', 'user'.
 * @param $entity
 *   The entity to be rendered.
 * @param string $plugin_id
 *   The Entity Embed Display plugin ID.
 * @param array $plugin_configuration
 *   (optional) Array of plugin configuration values.
 * @param array $context
 *   (optional) Array of additional context values, usually the embed HTML
 *   tag's attributes.
 *
 * @return array
 *   A render array for the Entity Embed Display plugin.
 */
function render_entity_embed_display_plugin($entity_type, $entity, $plugin_id, array $plugin_configuration = array(), array $context = array()) {
  // Check if the display plugin is accessible.
  if (!entity_access('view', $entity_type, $entity)) {
    return array();
  }

  // Split the formatter into separate parts.
  $formatter_parts = explode(':', $plugin_id);
  $formatter_module = $formatter_parts[0];
  $formatter_type = $formatter_parts[1];

  list($id, $vid, $bundle) = entity_extract_ids($entity_type, $entity);

  $field_formatter_info = field_info_formatter_types($formatter_type);

  // With the File Entity module enabled, file entities require some special
  // handling. Unlike standard field formatters which specify a list of
  // compatible entity types and will work with any entity as long as it is the
  // correct type, file formatters may only support files with specific MIME
  // types.
  if ($entity_type == 'file' && module_exists('file_entity')) {
    if (isset($field_formatter_info['file formatter']['mime types'])) {
      if (!file_entity_match_mimetypes($field_formatter_info['file formatter']['mime types'], $entity->filemime)) {
        $file_type = file_type_load($entity->type);

        // Inform the user that the file they are trying to embed cannot be
        // displayed using the selected formatter.
        return array(
          '#markup' => '<p>' . t('The %type file %filename is unable to be displayed as %formatter. Please select a different formatter.', array('%type' => $file_type->label, '%filename' => $entity->filename, '%formatter' => $field_formatter_info['label'])) . '</p>',
        );
      }
    }
  }

  if (isset($field_formatter_info['module'])) {
    // Set $display['type'] and $display['settings'] to what
    // hook_field_formatter_*() expects.
    $display['type'] = $formatter_type;
    $display['settings'] = $plugin_configuration += field_info_formatter_settings($formatter_type); // Use default settings if no settings are available.

    // Set $items to what the field formatter expects. See hook_field_load(),
    // and note that, here, $entity is already a fully loaded entity.
    $items = entity_embed_field_get_items($entity_type, $id, $field_formatter_info['module'], $context);

    // Invoke hook_field_formatter_prepare_view() and
    // hook_field_formatter_view(). Note that we are reusing field formatter
    // functions, but we are not displaying a Field API field, so we set
    // $field and $instance accordingly, and do not invoke
    // hook_field_prepare_view(). This assumes that the formatter functions do
    // not rely on $field or $instance. A module that implements formatter
    // functions that rely on $field or $instance (and therefore, can only be
    // used for real fields) can add support for being used on the pseudo-field
    // by adding in any missing information using the entity_embed_field_info
    // hooks.
    $field = entity_embed_field_info_field($entity_type, $field_formatter_info['module']);
    $instance = entity_embed_field_info_instance($formatter_type, $plugin_configuration);
    $langcode = entity_embed_get_langcode($context);

    if (($function = ($field_formatter_info['module'] . '_field_formatter_prepare_view')) && function_exists($function)) {
      // hook_field_formatter_prepare_view() alters $items by reference.
      $grouped_items = array($id => &$items);
      $function($entity_type, array($id => $entity), $field, array($id => $instance), $langcode, $grouped_items, array($id => $display));
    }
    if (($function = ($field_formatter_info['module'] . '_field_formatter_view')) && function_exists($function)) {
      $element = $function($entity_type, $entity, $field, $instance, $langcode, $items, $display);
      // We passed the entity as $items[0], so return the corresponding element.
      if (isset($element[0])) {
        return $element[0];
      }
    }
  }

  return array();
}

/**
 * Gets the current language code.
 *
 * @param array $context
 *   (optional) Array of context values, corresponding to the attributes on
 *   the embed HTML tag.
 *
 * @return string
 *   The langcode present in the 'data-langcode', if present, or the current
 *   langcode, otherwise.
 */
function entity_embed_get_langcode($context = array()) {
  global $language;
  $langcode = $language->language;

  if (!empty($context['data-langcode'])) {
    $langcode = $context['data-langcode'];
  }

  return $langcode;
}

/**
 * Get the possible field types that an entity might be displayed inside of.
 *
 * @param string $entity_type
 *   The type of entity that might be displayed.
 *
 * @return
 *   An array of field types that might display the passed entity type.
 */
function entity_embed_get_entity_field_types($entity_type) {
  $field_types = array();

  // All entity types, by definition, can be displayed as entityreference.
  $field_types[] = 'entityreference';

  // Support for nodes.
  if ('node' == $entity_type) {
    $field_types[] = 'node_reference';
  }

  // Support for users.
  if ('user' == $entity_type) {
    $field_types[] = 'user_reference';
  }

  // Support for taxonomy.
  if ('taxonomy_term' == $entity_type) {
    $field_types[] = 'taxonomy_term_reference';
  }

  // Support for files.
  if ('file' == $entity_type) {
    $field_types[] = 'image';
    $field_types[] = 'file';
  }

  // Support for any other entity_types.
  $additional_field_types = module_invoke_all('entity_embed_field_types', $entity_type);
  $field_types = array_merge($field_types, $additional_field_types);

  return $field_types;
}

/**
 * Helper function to get all the field formatters that we can use to display
 * entity types.
 *
 * @param $entity_type
 *   The type of entity, i.e. 'node', 'user'.
 * @param $entity
 *   (optional) The entity to be rendered. This is used to perform special
 *   checks/processing for unruly modules.
 *
 * @return array
 */
function entity_embed_get_entity_field_formatters($entity_type, $entity = NULL) {
  $formatters = field_info_formatter_types();
  $field_types = entity_embed_get_entity_field_types($entity_type);

  $entity_formatters = array();

  drupal_alter('entity_embed_field_formatters', $formatters, $entity_type, $entity);

  foreach ($formatters as $formatter => $info) {
    foreach ($info['field types'] as $field_type) {
      if (in_array($field_type, $field_types)) {
        $entity_formatters[$info['module'] . ':' . $formatter] = $info['label'];
        break;
      }
    }
  }

  return $entity_formatters;
}

/**
 * @file
 * Helper functions for Entity Embed involving field formatters.
 *
 * All of these functions are by definition hacky, because we're trying to use
 * a couple of hook in ways they were not intended to be used: displaying
 * arbitrary entities. The hooks are:
 * - hook_field_formatter_settings_form()
 * - hook_field_formatter_view()
 */

/**
 * Our own field-less and entity-less version of field_get_items().
 *
 * @param string $entity_type
 *   The entity type of the entity we are trying to embed.
 *
 * @param int $entity_id
 *   The entity id of the entity we are trying to embed.
 *
 * @param string $module
 *   The module that implements the field formatter we are going to use.
 *
 * @param array $context
 *   (optional) Array of context values, corresponding to the attributes on
 *   the embed HTML tag.
 *
 * In our version of field_get_items() we don't care at all about the field
 * or the entity that it is attached to. All we want is the output of the
 * formatter. So the point of this is return an $items array in the format
 * expected by the passed module's particular implementation of
 * hook_field_formatter_view().
 *
 * Ideally this would be a simple hook letting other modules do the work, but
 * we will hard-code support for the major players here.
 */
function entity_embed_field_get_items($entity_type, $entity_id, $module, $context = array()) {
  // Whatever module is involved, we're almost definitely going to need the
  // entity loaded and an access check.
  $entity = entity_load_single($entity_type, $entity_id);
  $access = entity_access('view', $entity_type, $entity);

  $item = array(
    'access' => $access,
  );

  // Support the entityreference module.
  if ('entityreference' == $module) {
    $item += array(
      'entity' => $entity,
      'target_id' => $entity_id,
    );
  }

  // Support the taxonomy module (core).
  elseif ('taxonomy' == $module) {
    $item += array(
      'tid' => $entity_id,
      'name' => entity_label($entity_type, $entity_id),
      'taxonomy_term' => $entity,
    );
  }

  // Support the file and image modules (core).
  elseif ('file' == $module || 'image' == $module) {
    // The file and image modules just expect an array version of the object.
    $item += (array) $entity;

    // Add alt + title text from the embed code to the field item, passing it
    // along to the field formatter. This enables both fields to be specified
    // per embed, instead of only per file entity.
    if (isset($context['alt'])) {
      $item['alt'] = $context['alt'];
    }

    if (isset($context['title'])) {
      $item['title'] = $context['title'];
    }
  }

  // Support the user_reference (references) module.
  elseif ('user_reference' == $module) {
    $item += array(
      'uid' => $entity_id,
      'user' => $entity,
    );
  }

  // Support the node_reference (references) module.
  elseif ('node_reference' == $module) {
    $item += array(
      'nid' => $entity_id,
      'node' => $entity,
    );
  }

  // Support the file_entity module.
  elseif ('file_entity' == $module) {
    // In general the file_entity module expects items to be
    // file objects, but in array form, similar to core image/file.
    $item += (array) $entity;
  }

  // If we didn't find anything, give other modules a chance.
  else {
    $contrib_items = module_invoke_all('entity_embed_field_get_items', $entity_type, $entity_id, $module, $context);
    // Make sure we only get 1 item.
    if (!empty($contrib_items)) {
      $item += array_pop($contrib_items);
    }
  }

  return array($item);
}

/**
 * Our own field-less entity-less version of field_get_display().
 *
 * @param $formatter_type
 *   A formatter type name.
 *
 * @param int $settings
 *   Optional array of settings to pass in. If omitted, the formatter's default
 *   settings will be returned.
 *
 * @return
 *   An array containing 'type' and 'settings'.
 *
 * In our version of field_get_display() we don't care at all about the field
 * or the entity/bundle that it is attached to. All we want is a display
 * array that will help us hack our way through hook_field_formatter_view() and
 * hook_field_formatter_settings_form(). This mainly means making sure the
 * 'settings' and 'type' elements are there and populated appropriately.
 *
 * Luckily this can be the same regardless of formatter/module, so we don't
 * need to hard-code module support or invoke any hooks.
 */
function entity_embed_field_get_display($formatter_type, $settings = array()) {
  // If settings is empty, we need to get defaults.
  if (empty($settings)) {
    $formatter_info = field_info_formatter_types($formatter_type);
    if (!empty($formatter_info['settings'])) {
      $settings = $formatter_info['settings'];
    }
  }

  return array(
    'type' => $formatter_type,
    'settings' => $settings,
  );
}

/**
 * Our own field-less version of field_info_field().
 *
 * @param string $entity_type
 *   The entity type of the entity we are trying to embed.
 *
 * @param string $module
 *   The module that implements the field formatter we are going to use.
 *
 * In our version of field_info_field() we don't care at all about the field
 * itself. All we want is the field info array in a format that lets us hack
 * into hook_field_formatter_view() and hook_field_formatter_settings_form().
 * The problem is that each module may expect something different in this array.
 *
 * Ideally this would be a simple hook letting other modules do the work, but
 * we will hard-code support for the major players here.
 */
function entity_embed_field_info_field($entity_type, $module) {
  $field = array();

  $field['field_name'] = '_entity_embed';

  // Support the entityreference module.
  if ('entityreference' == $module) {
    $field['type'] = 'entityreference';
    $field['settings'] = field_info_field_settings('entityreference');
    $field['settings']['target_type'] = $entity_type;
  }

  // Support the user_reference (references) module.
  elseif ('user_reference' == $module) {
    $field['type'] = 'user_reference';
    $field['settings'] = field_info_field_settings('user_reference');
  }

  // Support node_reference (references) module.
  elseif ('node_reference' == $module) {
    $field['type'] = 'node_reference';
    $field['settings'] = field_info_field_settings('node_reference');
  }

  // Support taxonomy (core) module.
  elseif ('taxonomy' == $module) {
    $field['type'] = 'taxonomy_term_reference';
    $field['settings'] = field_info_field_settings('taxonomy_term_reference');
  }

  // Support file (core) and file_entity modules.
  elseif ('file' == $module || 'file_entity' == $module) {
    $field['type'] = 'file';
    $field['settings'] = field_info_field_settings('file');
  }

  // Support file (core) module.
  elseif ('image' == $module) {
    $field['type'] = 'image';
    $field['settings'] = field_info_field_settings('image');
  }

  // If we didn't find anything, give other modules a chance.
  else {
    $field = module_invoke_all('entity_embed_field_info_field', $entity_type, $module);
  }

  return $field;
}

/**
 * Our own field-less entity-less version of field_info_instance().
 *
 * @param $formatter_type
 *   A formatter type name.
 *
 * @param int $settings
 *   Optional array of settings to pass in. If omitted, the formatter's default
 *   settings will be returned.
 *
 * In our version of field_info_instance() we don't care at all about the field
 * itself. All we want is the field instance array in a format that lets us hack
 * into hook_field_formatter_view() and hook_field_formatter_settings_form().
 * The problem is that each module may expect something different in this array.
 *
 * This is mainly a wrapper around entity_embed_field_get_display().
 */
function entity_embed_field_info_instance($formatter_type, $settings = array()) {
  return array(
    'display' => array(
      'default' => entity_embed_field_get_display($formatter_type, $settings),
    ),
  );
}

/**
 * Our own field-less entity-less version of field_info_formatter_settings().
 *
 * @param $entity
 *   The entity to be rendered.
 * @param $entity_element
 *   An associative array of properties taken from the embedded entity element.
 * @param $formatter_type
 *   A formatter type name.
 * @param $form
 *   A form array().
 * @param $form_state
 *   A form state array.
 *
 * In our version of field_info_formatter_settings() we don't care at all about
 * the field itself. All we want is the field info array in a format that lets
 * us hack hook_field_formatter_settings_form().
 */
function entity_embed_field_info_formatter_settings($entity, $entity_element, $formatter_type, $form, &$form_state) {
  $formatter_settings = array();

  $field_formatter_info = field_info_formatter_types($formatter_type);

  // Invoke hook_field_formatter_settings_form(). We are reusing field
  // formatter functions, but we are not working with a Field API field,
  // so set $field accordingly. Unfortunately, the API is for $settings to
  // be transferred via the $instance parameter, so we must mock it.
  if (isset($field_formatter_info['module']) && ($function = ($field_formatter_info['module'] . '_field_formatter_settings_form')) && function_exists($function)) {
    list($id, $vid, $bundle) = entity_extract_ids($entity_element['data-entity-type'], $entity);

    $field = entity_embed_field_info_field($entity_element['data-entity-type'], $field_formatter_info['module']);
    $view_mode = 'default';
    $mock_instance = array(
      'display' => array(
        $view_mode => array(
          'type' => $formatter_type,
          'settings' => $entity_element['data-entity-embed-display-settings'] + field_info_formatter_settings($formatter_type),
        ),
      ),
      'entity_type' => $entity_element['data-entity-type'],
      'bundle' => $bundle,
    );

    $formatter_settings += $function($field, $mock_instance, $view_mode, $form, $form_state);
  }

  entity_embed_build_configuration_form($formatter_settings, $entity, $entity_element, $formatter_type,  $form, $form_state);

  return $formatter_settings;
}

/**
 * Provides Entity Embed Display configuration for field formatters.
 *
 * @param $formatter_settings
 *   The form elements for the formatter settings.
 * @param $entity
 *   The entity to be rendered.
 * @param $entity_element
 *   An associative array of properties taken from the embedded entity element.
 * @param $formatter_type
 *   A formatter type name.
 * @param $form
 *   A form array().
 * @param $form_state
 *   A form state array.
 */
function entity_embed_build_configuration_form(&$formatter_settings, $entity, $entity_element, $formatter_type, $form, &$form_state) {
  $field_formatter_info = field_info_formatter_types($formatter_type);
  $field = entity_embed_field_info_field($entity_element['data-entity-type'], $field_formatter_info['module']);

  if ($entity_element['data-entity-type'] == 'file') {
    if ($field['type'] == 'file') {
      entity_embed_file_field_build_configuration_form($formatter_settings, $entity, $entity_element, $formatter_type, $form, $form_state);
    }
    if ($field['type'] == 'image') {
      entity_embed_file_image_build_configuration_form($formatter_settings, $entity, $entity_element, $formatter_type, $form, $form_state);
    }
  }
}

/**
 * Entity Embed Display reusing file field formatters.
 *
 * @param $formatter_settings
 *   The form elements for the formatter settings.
 * @param $entity
 *   The entity to be rendered.
 * @param $entity_element
 *   An associative array of properties taken from the embedded entity element.
 * @param $formatter_type
 *   A formatter type name.
 * @param $form
 *   A form array().
 * @param $form_state
 *   A form state array.
 */
function entity_embed_file_field_build_configuration_form(&$formatter_settings, $entity, $entity_element, $formatter_type, $form, &$form_state) {
//  // Description is stored in the configuration since it doesn't map to an
//  // actual HTML attribute.
//  $formatter_settings['description'] = array(
//    '#type' => 'textfield',
//    '#title' => t('Description'),
//    '#default_value' => $plugin_configuration['description'],
//    '#description' => t('The description may be used as the label of the link to the file.'),
//  );
}

/**
 * Entity Embed Display reusing image field formatters.
 *
 * @param $formatter_settings
 *   The form elements for the formatter settings.
 * @param $entity
 *   The entity to be rendered.
 * @param $entity_element
 *   An associative array of properties taken from the embedded entity element.
 * @param $formatter_type
 *   A formatter type name.
 * @param $form
 *   A form array().
 * @param $form_state
 *   A form state array.
 */
function entity_embed_file_image_build_configuration_form(&$formatter_settings, $entity, $entity_element, $formatter_type, $form, &$form_state) {
  // Ensure that the 'Link image to: Content' setting is not available.
  if (isset($formatter_settings['image_link'])) {
    unset($formatter_settings['image_link']['#options']['content']);
  }

  // The alt attribute is *required*, but we allow users to opt-in to empty
  // alt attributes for the very rare edge cases where that is valid by
  // specifying two double quotes as the alternative text in the dialog.
  // However, that *is* stored as an empty alt attribute, so if we're editing
  // an existing image (which means the src attribute is set) and its alt
  // attribute is empty, then we show that as two double quotes in the dialog.
  // @see https://www.drupal.org/node/2307647
  // Alt attribute behavior is taken from the Core image dialog to ensure a
  // consistent UX across various forms.
  // @see editor_image_dialog_form()
  $alt = isset($entity_element['alt']) ? $entity_element['alt'] : '';
  if ($alt === '') {
    // Do not change empty alt text to two double quotes if the previously
    // used Entity Embed Display plugin was not 'image:image'. That means that
    // some other plugin was used so if this image formatter is selected at a
    // later stage, then this should be treated as a new edit. We show two
    // double quotes in place of empty alt text only if that was filled
    // intentionally by the user.
    if (!empty($entity_element) && $entity_element['data-entity-embed-display'] == 'image:image') {
      $alt = ENTITY_EMBED_EMPTY_STRING;
    }
  }

  // Add support for editing the alternate and title text attributes.
  $formatter_settings['alt'] = array(
    '#type' => 'textfield',
    '#title' => t('Alternate text'),
    '#default_value' => $alt,
    '#description' => t('This text will be used by screen readers, search engines, or when the image cannot be loaded.'),
    '#parents' => array('attributes', 'alt'),
    '#required' => TRUE,
    '#maxlength' => 512,
  );
  $formatter_settings['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#default_value' => isset($entity_element['title']) ? $entity_element['title'] : '',
    '#description' => t('The title is used as a tool tip when the user hovers the mouse over the image.'),
    '#parents' => array('attributes', 'title'),
    '#maxlength' => 1024,
  );

  if (module_exists('file_entity')) {
    $langcode = entity_embed_get_langcode($entity_element);
    $field_alt = file_entity_replace_alt($entity, array(), NULL, $langcode);
    $field_title = file_entity_replace_title($entity, array(), NULL, $langcode);

    $formatter_settings['alt']['#placeholder'] = $field_alt;
    $formatter_settings['title']['#placeholder'] = $field_title;
  }
}
