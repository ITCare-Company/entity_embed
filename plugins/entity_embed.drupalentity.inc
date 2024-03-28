<?php

/**
 * @file
 * Defines the "drupalentity" plugin.
 */

/**
 * Config callback for the 'drupalentity' plugin.
 *
 * @param object $format
 *  A text format object.
 *
 * @return array
 *   An associative array of configuration settings.
 */
function editor_embed_drupalentity_config($format) {
  // Create a token for access to dialogs.
  $entity_embed_token = editor_dialog_token($format, 'entity_embed');

  // Add user-defined buttons.
  $embed_buttons = entity_embed_embed_button_load_all();

  $dialog_urls = array();
  $buttons = array();

  foreach ($embed_buttons as $embed_button) {
    $buttons[$embed_button->name] = array(
      'id' => check_plain($embed_button->name),
      'name' => check_plain($embed_button->admin_title),
      'label' => check_plain($embed_button->admin_title),
      'entity_type' => check_plain($embed_button->entity_type),
      'image' => check_plain(_entity_embed_button_image($embed_button->button_icon_fid)),
    );

    $dialog_urls[check_plain($embed_button->name)] = url('entity-embed/dialog/'. $format->format . '/' . check_plain($embed_button->name), array('query' => array(DIALOG_WRAPPER_FORMAT => 'drupal_modal', 'token' => $entity_embed_token, 'calling_path' => $_GET['q'])));
  }

  return array(
    'DrupalEntity_dialogTitleAdd' => t('Insert entity'),
    'DrupalEntity_dialogTitleEdit' => t('Edit entity'),
    'drupalEntity_dialogUrls' => $dialog_urls,
    'drupalEntity_previewUrl' => url('entity-embed/preview/' . $format->format, array('query' => array('token' => $entity_embed_token, 'calling_path' => $_GET['q']))),
    'DrupalEntity_buttons' => $buttons,
  );
}
