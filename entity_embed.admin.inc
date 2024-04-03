<?php

/**
 * @file
 * Admin page callbacks for the entity_embed module.
 */

/**
 * Loads an entity (in the appropriate translation) given HTML attributes.
 *
 * @param array $attributes
 *   An array of HTML attributes, including at least `data-entity-type` and
 *   `data-entity-uuid`, and optionally `data-langcode`.
 *
 * @return object|null
 *   The requested entity, or NULL.
 */
function entity_embed_load_entity_by_attributes(array $attributes) {
  if (!empty($attributes['data-entity-uuid']) && module_exists('uuid')) {
    $entity = entity_uuid_load($attributes['data-entity-type'], array($attributes['data-entity-uuid']));
  }
  else {
    $entity = entity_load($attributes['data-entity-type'], array($attributes['data-entity-id']));
  }

  $entity = reset($entity);

  return $entity;
}

/**
 * Form constructor for the entity embed dialog form.
 *
 * Allows the user to select and embed an entity.
 *
 * @see entity_embed_dialog_form_validate()
 * @ingroup forms
 */
function entity_embed_dialog_form($form, &$form_state, $filter_format, $embed_button) {
  $values = isset($form_state['values']) ? $form_state['values'] : array();
  $input = isset($form_state['input']) ? $form_state['input'] : array();

  // Set embed button element in form state, so that it can be used later in
  // validateForm() function.
  $form_state['embed_button'] = $embed_button;
  $form_state['filter_format'] = $filter_format;

  // Initialize entity element with form attributes, if present.
  $entity_element = empty($values['attributes']) ? array() : $values['attributes'];
  $entity_element += empty($input['attributes']) ? array() : $input['attributes'];

  // The default values are set directly from \Drupal::request()->request,
  // provided by the editor plugin opening the dialog.
  if (empty($form_state['entity_element'])) {
    $form_state['entity_element'] = isset($input['editor_object']) ? $input['editor_object'] : array();
  }

  $entity_element += $form_state['entity_element'];
  $entity_element += array(
    'data-entity-type' => $embed_button->entity_type,
    'data-entity-uuid' => '',
    'data-entity-id' => '',
    'data-entity-embed-display' => 'entityreference:entityreference_entity_view',
    'data-entity-embed-display-settings' => isset($form_state['entity_element']['data-entity-embed-settings']) ? $form_state['entity_element']['data-entity-embed-settings'] : array(),
  );
  $form_state['entity_element'] = $entity_element;
  $entity = entity_embed_load_entity_by_attributes($entity_element);
  $form_state['entity'] = isset($entity) ? $entity : NULL;

  if (empty($form_state['step'])) {
    // If an entity has been selected, then always skip to the embed options.
    if (!empty($form_state['entity'])) {
      $form_state['step'] = 'embed';
    }
    else {
      $form_state['step'] = 'select';
    }
  }

  $form['#tree'] = TRUE;
  $form['#attached']['library'][] = array('editor', 'editor.dialog');
  $form['#prefix'] = '<div id="entity-embed-dialog-form">';
  $form['#suffix'] = '</div>';
  $form['#attributes']['class'][] = 'entity-embed-dialog-step--' . $form_state['step'];

  if ($form_state['step'] == 'select') {
    $form = entity_embed_build_select_step($form, $form_state);
  }
  elseif ($form_state['step'] == 'embed') {
    $form = entity_embed_build_embed_step($form, $form_state);
  }

  return $form;
}

/**
 * Form constructor for the entity selection step.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   The current state of the form.
 *
 * @return array
 *   The form structure.
 */
function entity_embed_build_select_step($form, &$form_state) {
  $form['#title'] = t('Embed entity');

  // Entity element is calculated on every AJAX request/submit.
  // See entity_embed_dialog_form().
  $entity_element = $form_state['entity_element'];
  $embed_button = $form_state['embed_button'];
  $entity = $form_state['entity'];

  $form['attributes']['data-entity-type'] = array(
    '#type' => 'value',
    '#value' => $entity_element['data-entity-type'],
  );

  $label = t('Label');
  // Attempt to display a better label if we can by getting it from
  // the label field definition.
  $entity_info = entity_get_info($entity_element['data-entity-type']);

  if (!empty($entity_info['label'])) {
    $label = check_plain($entity_info['label']);
  }

  $default = '';

  if ($entity) {
    $default = entity_label($entity_element['data-entity-type'], $entity) . ' [id: ' . $entity_element['data-entity-id'] . ']';
  }

  $form['entity_id'] = array(
    '#type' => 'textfield',
    '#title' => $label,
    '#default_value' => $default,
    '#autocomplete_path' => 'ctools/autocomplete/' . $embed_button->entity_type,
    '#required' => TRUE,
    '#description' => t('Type label and pick the right one from suggestions. Note that the unique ID will be saved.'),
  );

  // Integrate with the Media module to support selecting files to embed.
  if ($entity_element['data-entity-type'] == 'file' && module_exists('media')) {
    $form['entity_id'] = array(
      '#type' => 'media',
      '#title' => $label,
      '#default_value' => $entity_element['data-entity-id'],
      '#required' => TRUE,
      '#description' => t('Select a file to embed using the media browser.'),
      '#media_options' => array(
        'global' => array(
          'types' => isset($embed_button->entity_type_bundles) ? $embed_button->entity_type_bundles : array(),
        ),
      ),
    );
  }

  if (!empty($entity_element['data-langcode'])) {
    $form['attributes']['data-langcode'] = array(
      '#type' => 'hidden',
      '#value' => $entity_element['data-langcode'],
    );
  }

  $form['attributes']['data-entity-id'] = array(
    '#type' => 'value',
    '#title' => $entity_element['data-entity-id'],
  );
  $form['attributes']['data-entity-uuid'] = array(
    '#type' => 'value',
    '#title' => $entity_element['data-entity-uuid'],
  );
  $form['actions'] = array(
    '#type' => 'actions',
  );
  $form['actions']['save_modal'] = array(
    '#attributes' => array(
      'class' => array('button--primary'),
    ),
    '#type' => 'submit',
    '#value' => t('Next'),
    // No regular submit-handler. This form only works via JavaScript.
    '#submit' => array(),
    '#ajax' => array(
      'callback' => 'submitSelectForm',
      'event' => 'click',
    ),
  );

  return $form;
}

/**
 * Form constructor for the entity embedding step.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   The current state of the form.
 *
 * @return array
 *   The form structure.
 */
function entity_embed_build_embed_step($form, &$form_state) {
  // Entity element is calculated on every AJAX request/submit.
  // See entity_embed_dialog_form().
  $entity_element = $form_state['entity_element'];
  $embed_button = $form_state['embed_button'];
  $filter_format = $form_state['filter_format'];
  $entity = $form_state['entity'];
  $values = isset($form_state['values']) ? $form_state['values'] : array();

  $form['#title'] = t('Edit embedded entity');

  $text = t('Preview selected entity');
  if ($entity) {
    $text = entity_label($entity_element['data-entity-type'], $entity);
  }

  $uri = entity_uri($entity_element['data-entity-type'], $entity);

  $form['entity'] = array(
    '#type' => 'item',
    '#title' => t('Selected entity'),
    '#markup' => l($text, $uri['path'], $uri['options'] + array('attributes' => array('target' => '_blank'))),
  );
  $form['attributes']['data-entity-type'] = array(
    '#type' => 'value',
    '#value' => $entity_element['data-entity-type'],
  );
  $form['attributes']['data-entity-id'] = array(
    '#type' => 'value',
    '#value' => $entity_element['data-entity-id'],
  );

  if (!empty($entity_element['data-langcode'])) {
    $form['attributes']['data-langcode'] = array(
      '#type' => 'hidden',
      '#value' => $entity_element['data-langcode'],
    );
  }

  $form['attributes']['data-entity-uuid'] = array(
    '#type' => 'value',
    '#value' => $entity_element['data-entity-uuid'],
  );

  // Determine the available Display plugins for the current entity type.
  $available_plugins = entity_embed_get_entity_field_formatters($entity_element['data-entity-type'], $entity);

  // Retrieve the list of allowed Entity Embed Display plugins.
  $allowed_plugins = $embed_button->display_plugins;

  // If list of allowed options is empty, it means that all plugins are
  // allowed. Else, take the intersection of allowed and available
  // plugins.
  $display_plugin_options = empty($allowed_plugins) ? $available_plugins : array_intersect_key($available_plugins, $allowed_plugins);

  // If the currently selected display is not in the available options,
  // use the first from the list instead. This can happen if an alter
  // hook customizes the list based on the entity.
  if (!isset($display_plugin_options[$entity_element['data-entity-embed-display']])) {
    $entity_element['data-entity-embed-display'] = key($display_plugin_options);
  }

  // The default Entity Embed Display plugin has been deprecated by the
  // rendered entity field formatter.
  if ($entity_element['data-entity-embed-display'] === 'default') {
    $entity_element['data-entity-embed-display'] = 'entityreference:entityreference_entity_view';
  }

  // @todo: Abstract formatters into 'display plugins'.
  $form['attributes']['data-entity-embed-display'] = array(
    '#type' => 'select',
    '#title' => t('Display as'),
    '#options' => $display_plugin_options,
    '#default_value' => $entity_element['data-entity-embed-display'],
    '#required' => TRUE,
    '#ajax' => array(
      'callback' => 'updatePluginConfigurationForm',
      'wrapper' => 'data-entity-embed-display-settings-wrapper',
      'effect' => 'fade',
    ),
    // Hide the selection if only one option is available.
    '#access' => count($display_plugin_options) > 1,
  );
  $form['attributes']['data-entity-embed-display-settings'] = array(
    '#type' => 'container',
    '#prefix' => '<div id="data-entity-embed-display-settings-wrapper">',
    '#suffix' => '</div>',
  );
  $form['attributes']['data-embed-button'] = array(
    '#type' => 'value',
    '#value' => $embed_button->name,
  );
  $form['attributes']['data-entity-label'] = array(
    '#type' => 'value',
    '#value' => $embed_button->admin_title,
  );

  $plugin_id = !empty($values['attributes']['data-entity-embed-display']) ? $values['attributes']['data-entity-embed-display'] : $entity_element['data-entity-embed-display'];

  // Split the formatter into separate parts.
  $formatter_parts = explode(':', $plugin_id);
  $formatter_module = $formatter_parts[0];
  $formatter_type = $formatter_parts[1];

  // Retrieve the settings appropriate for the currently selected formatter.
  if (!empty($formatter_type)) {

    if (empty($entity_element['data-entity-embed-display-settings'])) {
      $entity_element['data-entity-embed-display-settings'] = array();
    }
    elseif (is_string($entity_element['data-entity-embed-display-settings'])) {
      $entity_element['data-entity-embed-display-settings'] = drupal_json_decode($entity_element['data-entity-embed-display-settings']) ? drupal_json_decode($entity_element['data-entity-embed-display-settings']) : array();
    }

    $form['attributes']['data-entity-embed-display-settings'] += entity_embed_field_info_formatter_settings($entity, $entity_element, $formatter_type, $form, $form_state);
  }

  $filters = filter_list_format($filter_format->format);

  // When Editor's editor_align is being used, the text editor may offer the
  // ability to change the alignment.
  if (isset($filters['editor_align']) && $filters['editor_align']->status) {
    $form['attributes']['data-align'] = array(
      '#title' => t('Align'),
      '#type' => 'radios',
      '#options' => array(
        'none' => t('None'),
        'left' => t('Left'),
        'center' => t('Center'),
        'right' => t('Right'),
      ),
      '#default_value' => isset($entity_element['data-align']) ? $entity_element['data-align'] : '',
      '#wrapper_attributes' => array('class' => array('container-inline')),
      '#attributes' => array('class' => array('container-inline')),
    );
  }

  // When Editor's editor_caption is being used, the text editor may offer
  // the ability to add a caption.
  if (isset($filters['editor_caption']) && $filters['editor_caption']->status) {
    $form['attributes']['data-caption'] = array(
      '#title' => t('Caption'),
      '#type' => 'textarea',
      '#rows' => 3,
      '#default_value' => isset($entity_element['data-caption']) ? decode_entities($entity_element['data-caption']) : '',
      '#element_validate' => array('entity_embed_escape_value'),
    );
  }

  $form['actions'] = array(
    '#type' => 'actions',
  );
  $form['actions']['back'] = array(
    '#type' => 'submit',
    '#value' => t('Back'),
    // No regular submit-handler. This form only works via JavaScript.
    '#submit' => array(),
    '#ajax' => array(
      'callback' => 'goBack',
      'event' => 'click',
    ),
  );
  $form['actions']['save_modal'] = array(
    '#attributes' => array(
      'class' => array('button--primary'),
    ),
    '#type' => 'submit',
    '#value' => t('Embed'),
    // No regular submit-handler. This form only works via JavaScript.
    '#submit' => array(),
    '#ajax' => array(
      'callback' => 'submitEmbedForm',
      'event' => 'click',
    ),
  );

  return $form;
}

/**
 * Form validation handler for entity_embed_dialog_form().
 *
 * @see entity_embed_dialog_form()
 */
function entity_embed_dialog_form_validate($form, &$form_state) {
  if ($form_state['step'] == 'select') {
    entity_embed_validate_select_step($form, $form_state);
  }
  elseif ($form_state['step'] == 'embed') {
    entity_embed_validate_embed_step($form, $form_state);
  }
}

/**
 * Form validation handler for the entity selection step.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   The current state of the form.
 */
function entity_embed_validate_select_step($form, &$form_state) {
  $id = trim($form_state['values']['entity_id']);
  $element = $form['entity_id'];

  $entity_type = $form_state['values']['attributes']['data-entity-type'];

  if (!isset($id)) {
    form_error($element, t('No entity selected.'));
    return;
  }

  $preg_matches = array();
  $match = preg_match('/\[id: (\d+)\]/', $id, $preg_matches);

  if (!$match) {
    $match = preg_match('/^id: (\d+)/', $id, $preg_matches);
  }

  if ($match) {
    $id = $preg_matches[1];
  }

  $entity_info = entity_get_info($entity_type);

  // Prevent errors if the entity type has no label key.
  if (empty($entity_info['entity keys']['label'])) {
    form_set_error('data-entity-id', t('Unable to find label for @type entity @id.', array('@type' => $entity_type, '@id' => $id)));
  }

  // The entity ID may be either the ID (integer) of the entity or the
  // entity's title (string).
  if (is_numeric($id)) {
    $entities = entity_load($entity_type, array($id));
  }
  else {
    $entities = entity_load($entity_type, FALSE, array($entity_info['entity keys']['label'] => array($id)));
  }

  $entity = reset($entities);

  if (!empty($entity)) {
    list($id, $vid, $bundle) = entity_extract_ids($entity_type, $entity);

    if (!entity_access('view', $entity_type, $entity)) {
      form_error($element, t('Unable to access @type entity @id.', array('@type' => $entity_type, '@id' => $id)));
    }
    else {
      form_set_value($form['attributes']['data-entity-id'], $id, $form_state);

      if (isset($entity->uuid)) {
        form_set_value($form['attributes']['data-entity-uuid'], $entity->uuid, $form_state);
      }
      else {
        form_set_value($form['attributes']['data-entity-uuid'], '', $form_state);
      }

      // Ensure that at least one Entity Embed Display plugin is present
      // before proceeding to the next step. Raise an error otherwise.
      $embed_button = $form_state['embed_button'];

      // Determine the available Display plugins for the current entity type.
      $available_plugins = entity_embed_get_entity_field_formatters($entity_type, $entity);

      // Retrieve the list of allowed Entity Embed Display plugins.
      $allowed_plugins = $embed_button->display_plugins;

      // If list of allowed options is empty, it means that all plugins are
      // allowed. Else, take the intersection of allowed and available
      // plugins.
      $display_plugin_options = empty($allowed_plugins) ? $available_plugins : array_intersect_key($available_plugins, $allowed_plugins);

      // If no plugin is available after taking the intersection,
      // raise error. Also log an exception.
      if (empty($display_plugin_options)) {
        form_error($element, t('No display options available for the selected %entity-type. Please select another %entity_type.', array('%entity_type' => $entity_info['entity keys']['label'])));
        watchdog('entity_embed', 'No display options available for "@type:" entity "@id" while embedding using button "@button". Please ensure that at least one Entity Embed Display plugin is allowed for this embed button which is available for this entity.', array('@type' => $entity_type, '@id' => $id, '@button' => $embed_button->admin_title), WATCHDOG_WARNING);
      }
    }
  }
  else {
    form_set_error('data-entity-id', t('Unable to load @type entity @id.', array('@type' => $entity_type, '@id' => $id)));
  }
}

/**
 * Form validation handler for the entity embedding step.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   The current state of the form.
 */
function entity_embed_validate_embed_step($form, &$form_state) {
  // Validate configuration forms for the Entity Embed Display plugin used.
  $entity_element = $form_state['values']['attributes'];
  $entity = $form_state['entity'];
  $plugin_id = $entity_element['data-entity-embed-display'];
  $plugin_settings = !empty($entity_element['data-entity-embed-display-settings']) ? $entity_element['data-entity-embed-display-settings'] : array();
  // @todo: Display plugin validation.

  if (isset($entity_element['alt']) && $entity_element['alt'] == '') {
    form_set_error('attributes[alt]', t('Alternative text is empty.<br />(Only in rare cases should this be left empty. To create empty alternative text, enter <code>""</code> — two double quotes without any content).'));
  }
}

/**
 * Form submission handler to update the plugin configuration form.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   An associative array containing the current state of the form.
 */
function updatePluginConfigurationForm(&$form, $form_state) {
  return $form['attributes']['data-entity-embed-display-settings'];
}

/**
 * Form submission handler to go back to the previous step of the form.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   An associative array containing the current state of the form.
 */
function goBack(&$form, $form_state) {
  $form_state['step'] = 'select';
  $form_state['rebuild'] = TRUE;
  $rebuild_form = drupal_rebuild_form('entity_embed_dialog_form', $form_state, $form);
  unset($rebuild_form['#prefix'], $rebuild_form['#suffix']);
  $status_messages = array('#theme' => 'status_messages');
  $output = drupal_render($rebuild_form);
  drupal_process_attached($rebuild_form);
  $output = '<div>' . drupal_render($status_messages) . $output . '</div>';
  $commands[] = ajax_command_html('#entity-embed-dialog-form', $output);

  return array(
    '#type' => 'ajax',
    '#commands' => $commands,
  );
}

/**
 * Form submission handler for the entity selection step.
 *
 * On success will send the user to the next step of the form to select the
 * embed display settings. On form errors, this will rebuild the form and
 * display the error messages.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   An associative array containing the current state of the form.
 */
function submitSelectForm(&$form, $form_state) {
  // Display errors in form, if any.
  if (form_get_errors()) {
    unset($form['#prefix'], $form['#suffix']);
    $status_messages = array('#theme' => 'status_messages');
    $output = drupal_render($form);
    $output = '<div>' . drupal_render($status_messages) . $output . '</div>';
    $commands[] = ajax_command_html('#entity-embed-dialog-form', $output);
  }
  else {
    $form_state['step'] = 'embed';
    $form_state['rebuild'] = TRUE;
    $rebuild_form = drupal_rebuild_form('entity_embed_dialog_form', $form_state, $form);
    unset($rebuild_form['#prefix'], $rebuild_form['#suffix']);
    $status_messages = array('#theme' => 'status_messages');
    $output = drupal_render($rebuild_form);
    drupal_process_attached($rebuild_form);
    $output = '<div>' . drupal_render($status_messages) . $output . '</div>';
    $commands[] = ajax_command_html('#entity-embed-dialog-form', $output);
  }

  return array(
    '#type' => 'ajax',
    '#commands' => $commands,
  );
}

/**
 * Form submission handler for the entity embedding step.
 *
 * On success this will submit the command to save the embedded entity with
 * the configured display settings to the WYSIWYG element, and then close the
 * modal dialog. On form errors, this will rebuild the form and display the
 * error messages.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   An associative array containing the current state of the form.
 */
function submitEmbedForm(&$form, $form_state) {
  // Submit configuration form the selected Entity Embed Display plugin.
  $entity_element = $form_state['values']['attributes'];
  $entity = entity_embed_load_entity_by_attributes($entity_element);
  // @todo: Display plugin submission.
  $plugin_id = $entity_element['data-entity-embed-display'];
  $plugin_settings = !empty($entity_element['data-entity-embed-display-settings']) ? $entity_element['data-entity-embed-display-settings'] : array();

  // When the alt attribute is set to two double quotes, transform it to the
  // empty string: two double quotes signify "empty alt attribute". See above.
  if (isset($entity_element['alt']) && trim($entity_element['alt']) === ENTITY_EMBED_EMPTY_STRING) {
    $form_state['values']['attributes']['alt'] = '';
  }

  $values = $form_state['values'];

  // Display errors in form, if any.
  if (form_get_errors()) {
    unset($form['#prefix'], $form['#suffix']);
    $status_messages = array('#theme' => 'status_messages');
    $output = drupal_render($form);
    $output = '<div>' . drupal_render($status_messages) . $output . '</div>';
    $commands[] = ajax_command_html('#entity-embed-dialog-form', $output);
  }
  else {
    // Serialize entity embed settings to JSON string.
    if (!empty($values['attributes']['data-entity-embed-display-settings'])) {
      $values['attributes']['data-entity-embed-display-settings'] = drupal_json_encode($values['attributes']['data-entity-embed-display-settings']);
    }

    // Filter out empty attributes.
    $values['attributes'] = array_filter($values['attributes'], function ($value) {
      return (bool) mb_strlen((string) $value);
    });

    // Allow other modules to alter the values before getting submitted to the
    // WYSIWYG.
    drupal_alter('entity_embed_values', $values, $entity, $display);

    $commands[] = editor_command_editor_dialog_save($values);
    $commands[] = dialog_command_close_modal_dialog();
  }

  return array(
    '#type' => 'ajax',
    '#commands' => $commands,
  );
}

/**
 * Form element validation handler; Escapes the value an element.
 *
 * This should be used for any element in the embed form which may contain
 * HTML that should be serialized as an attribute element on the embed.
 */
function entity_embed_escape_value($element, &$form_state) {
  if ($value = trim($element['#value'])) {
    form_set_value($element, check_plain($value), $form_state);
  }
}
