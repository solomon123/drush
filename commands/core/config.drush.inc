<?php

/**
 * @file
 *   Provides Configuration Management commands.
 */

use Drupal\Core\Config\StorageComparer;
use Drupal\Core\Config\ConfigImporter;
use Drupal\Core\Config\FileStorage;
use Symfony\Component\Yaml\Parser;

/**
 * Implementation of hook_drush_help().
 */
function config_drush_help($section) {
  switch ($section) {
    case 'meta:config:title':
      return dt('Config commands');
    case 'meta:config:summary':
      return dt('Interact with the configuration system.');
  }
}

/**
 * Implementation of hook_drush_command().
 */
function config_drush_command() {
  $items['config-get'] = array(
    'description' => 'Display a config value, or a whole configuration object.',
    'arguments' => array(
      'config-name' => 'The config object name, for example "system.site".',
      'key' => 'The config key, for example "page.front". Optional.',
    ),
    'required-arguments' => 1,
    'options' => array(
      'source' => array(
        'description' => 'The config storage source to read. recognized values are \'active \'and \'staging\'.',
        'example-value' => 'active',
        'value' => 'required',
      ),
      'include-overridden' => array(
        'description' => 'Include overridden values.',
      )
    ),
    'examples' => array(
      'drush config-get system.site' => 'Displays the system.site config.',
      'drush config-get system.site page.front' => 'gets system.site:page.front value.',
    ),
    'outputformat' => array(
      'default' => 'yaml',
      'pipe-format' => 'var_export',
    ),
    'aliases' => array('cget'),
    'core' => array('8+'),
  );

  $items['config-set'] = array(
    'description' => 'Set config value directly in active configuration.',
    'arguments' => array(
      'config-name' => 'The config object name, for example "system.site".',
      'key' => 'The config key, for example "page.front".',
      'value' => 'The value to assign to the config key. Use \'-\' to read from STDIN.',
    ),
    'options' => array(
      'format' => array(
        'description' => 'Format to parse the object. Use "string" for string (default), and "yaml" for YAML.',
        'example-value' => 'yaml',
        'value' => 'required',
      ),
      // A convenient way to pass a multiline value within a backend request.
      'value' => array(
        'description' => 'The value to assign to the config key (if any).',
        'hidden' => TRUE,
      ),
    ),
    'examples' => array(
      'drush config-set system.site page.front node' => 'Sets system.site:page.front to "node".',
    ),
    'aliases' => array('cset'),
    'core' => array('8+'),
  );

  $items['config-export'] = array(
    'description' => 'Export config from the active directory.',
    'core' => array('8+'),
    'aliases' => array('cex'),
    'arguments' => array(
      'label' => "A config directory label (i.e. a key in \$config_directories array in settings.php). Defaults to 'staging'",
    ),
    'options' => array(
      'add' => 'Run `git add -p` after exporting. This lets you choose which config changes to stage for commit.',
      'destination' => 'An arbitrary directory that should receive the exported files. An alternative to label argument',
    ),
  );

  $items['config-import'] = array(
    'description' => 'Import config from a config directory.',
    'arguments' => array(
      'label' => "A config directory label (i.e. a key in \$config_directories array in settings.php). Defaults to 'staging'",
    ),
    'options' => array(
      'preview' => array(
        'description' => 'Format for displaying proposed changes. Recognized values: list, diff. Defaults to list',
        'example-value' => 'list',
      ),
      'source' => 'An arbitrary directory that holds the configuration files. An alternative to label argument',
      'partial' => 'Allows for partial config imports from the source directory. Only updates and new configs will be processed with this flag (missing configs will not be deleted).',
    ),
    'core' => array('8+'),
    'aliases' => array('cim'),
  );

  $items['config-list'] = array(
    'description' => 'List config names by prefix.',
    'core' => array('8+'),
    'aliases' => array('cli'),
    'arguments' => array(
      'prefix' => 'The config prefix. For example, "system". No prefix will return all names in the system.',
    ),
    'examples' => array(
      'drush config-list system' => 'Return a list of all system config names.',
      'drush config-list "image.style"' => 'Return a list of all image styles.',
      'drush config-list --format="json"' => 'Return all config names as json.',
    ),
    'outputformat' => array(
      'default' => 'list',
      'pipe-format' => 'var_export',
      'output-data-type' => 'format-list',
    ),
  );

  $items['config-edit'] = array(
    'description' => 'Open a config file in a text editor. Edits are imported into active configration after closing editor.',
    'core' => array('8+'),
    'aliases' => array('cedit'),
    'arguments' => array(
      'config-name' => 'The config object name, for example "system.site".',
    ),
    'options' => array(
      'bg' => 'Run editor in the background. Does not work with editors such as `vi` that run in the terminal. Supresses config-import at the end.',
      'file' => 'Import from a file instead of interactively editing a given config.',
    ),
    'examples' => array(
      'drush config-edit image.style.large' => 'Edit the image style configurations.',
      'drush config-edit' => 'Choose a config file to edit.',
      'drush config-edit --choice=2' => 'Edit the second file in the choice list.',
      'drush --bg config-edit image.style.large' => 'Return to shell prompt as soon as the editor window opens.',
    ),
  );

  return $items;
}

/**
 * Config list command callback
 *
 * @param string $prefix
 *   The config prefix to retrieve, or empty to return all.
 */
function drush_config_list($prefix = '') {
  $names = \Drupal::configFactory()->listAll($prefix);

  if (empty($names)) {
    // Just in case there is no config.
    if (!$prefix) {
      return drush_set_error(dt('No config storage names found.'));
    }
    else {
      return drush_set_error(dt('No config storage names found matching @prefix', array('@prefix' => $prefix)));
    }
  }

  return $names;
}

/**
 * Config get command callback.
 *
 * @param $config_name
 *   The config name.
 * @param $key
 *   The config key.
 */
function drush_config_get($config_name, $key = NULL) {
  if (!isset($key)) {
    return drush_config_get_object($config_name);
  }
  else {
    return drush_config_get_value($config_name, $key);
  }
}

/**
 * Config set command callback.
 *
 * @param $config_name
 *   The config name.
 * @param $key
 *   The config key.
 * @param $data
 *    The data to save to config.
 */
function drush_config_set($config_name, $key = NULL, $data = NULL) {
  // This hidden option is a convenient way to pass a value without passing a key.
  $data = drush_get_option('value', $data);

  if (!isset($data)) {
    return drush_set_error('DRUSH_CONFIG_ERROR', dt('No config value specified.'));
  }

  $config = Drupal::configFactory()->getEditable($config_name);
  // Check to see if config key already exists.
  if ($config->get($key) === NULL) {
    $new_key = TRUE;
  }
  else {
    $new_key = FALSE;
  }

  // Special flag indicating that the value has been passed via STDIN.
  if ($data === '-') {
    $data = stream_get_contents(STDIN);
  }

  // Now, we parse the value.
  switch (drush_get_option('format', 'string')) {
    case 'yaml':
      $parser = new Parser();
      $data = $parser->parse($data, TRUE);
  }

  if (is_array($data) && drush_confirm(dt('Do you want to update or set multiple keys on !name config.', array('!name' => $config_name)))) {
    foreach ($data as $key => $value) {
      $config->set($key, $value);
    }
    return $config->save();
  }
  else {
    $confirmed = FALSE;
    if ($config->isNew() && drush_confirm(dt('!name config does not exist. Do you want to create a new config object?', array('!name' => $config_name)))) {
      $confirmed = TRUE;
    }
    elseif ($new_key && drush_confirm(dt('!key key does not exist in !name config. Do you want to create a new config key?', array('!key' => $key, '!name' => $config_name)))) {
      $confirmed = TRUE;
    }
    elseif (drush_confirm(dt('Do you want to update !key key in !name config?', array('!key' => $key, '!name' => $config_name)))) {
      $confirmed = TRUE;
    }
    if ($confirmed && !drush_get_context('DRUSH_SIMULATE')) {
      return $config->set($key, $data)->save();
    }
  }
}

function drush_config_export_validate() {
  if ($destination = drush_get_option('destination')) {
    if (!file_exists($destination)) {
      return drush_set_error('config_export_target', 'The destination directory does not exist.');
    }
    if (!is_dir($destination)) {
      return drush_set_error('config_export_target', 'The destination is not a directory.');
    }
    if (!is_writable($destination)) {
      return drush_set_error('config_export_target', 'The destination directory is not writable.');
    }
  }
}

/**
 * Command callback: Export config to specified directory (usually staging).
 */
function drush_config_export($destination = NULL) {
  global $config_directories;

  if ($target = drush_get_option('destination')) {
    $destination_dir = $target;
  }
  else {
    $choices = drush_map_assoc(array_keys($config_directories));
    unset($choices[CONFIG_ACTIVE_DIRECTORY]);
    if (!isset($destination) && count($choices) >= 2) {
      $destination = drush_choice($choices, 'Choose a destination.');
      if (empty($destination)) {
        return drush_user_abort();
      }
    }
    elseif (!isset($destination)) {
      $destination = CONFIG_STAGING_DIRECTORY;
    }
    $destination_dir = config_get_config_directory($destination);
  }

  if (count(glob($destination_dir . '/*')) > 0) {
    if (!drush_confirm(dt('The current contents of your export directory (!target) will be deleted.', array('!target' => $destination_dir)))) {
      return drush_user_abort();
    }
    // Only delete .yml files, and not .htaccess or .git.
    drush_scan_directory($destination_dir, '/\.yml$/', array('.', '..'), 'unlink');
  }

  // Write all .yml files.
  $source_storage = Drupal::service('config.storage');
  $destination_storage = new FileStorage($destination_dir);
  foreach ($source_storage->listAll() as $name) {
    $destination_storage->write($name, $source_storage->read($name));
  }

  // Export configuration collections.
  foreach (\Drupal::service('config.storage')->getAllCollectionNames() as $collection) {
    $source_storage = $source_storage->createCollection($collection);
    $destination_storage = $destination_storage->createCollection($collection);
    foreach ($source_storage->listAll() as $name) {
      $destination_storage->write($name, $source_storage->read($name));
    }
  }

  drush_log(dt('Configuration successfully exported to !target.', array('!target' => $destination_dir)), 'success');
  drush_backend_set_result($destination_dir);

  if (drush_get_option('add')) {
    drush_shell_exec_interactive('git add -p %s', $destination_dir);
  }
}

function drush_config_import_validate() {
  if ($source = drush_get_option('source')) {
    if (!file_exists($source)) {
      return drush_set_error('config_import_target', 'The source directory does not exist.');
    }
    if (!is_dir($source)) {
      return drush_set_error('config_import_target', 'The source is not a directory.');
    }
  }
}

/**
 * Command callback. Import from specified config directory (defaults to staging).
 */
function drush_config_import($source = NULL) {
  global $config_directories;

  if ($target = drush_get_option('source')) {
    $source_dir = $target;
  }
  else {
    $choices = drush_map_assoc(array_keys($config_directories));
    unset($choices[CONFIG_ACTIVE_DIRECTORY]);
    if (!isset($source) && count($choices) >= 2) {
      $source= drush_choice($choices, 'Choose a source.');
      if (empty($source)) {
        return drush_user_abort();
      }
    }
    elseif (!isset($source)) {
      $source = CONFIG_STAGING_DIRECTORY;
    }
    $source_dir = config_get_config_directory($source);
  }

  // Retrieve a list of differences between the active and source configuration (if any).
  $source_storage = new FileStorage($source_dir);
  /** @var \Drupal\Core\Config\StorageInterface $active_storage */
  $active_storage = Drupal::service('config.storage');
  if (drush_get_option('partial', FALSE)) {
    // With partial imports, the comparison must only be made against configs
    // that exist in the source directory.
    $temp_active_storage = new FileStorage(drush_tempdir());
    foreach ($source_storage->listAll() as $name) {
      // Copy active storage to our temporary active store.
      if ($existing = $active_storage->read($name)) {
        $temp_active_storage->write($name, $existing);
      }
    }
    $active_storage = $temp_active_storage;
  }
  $config_comparer = new StorageComparer($source_storage, $active_storage, Drupal::service('config.manager'));
  if (!$config_comparer->createChangelist()->hasChanges()) {
    return drush_log(dt('There are no changes to import.'), 'ok');
  }

  if (drush_get_option('preview', 'list') == 'list') {
    $change_list = array();
    foreach ($config_comparer->getAllCollectionNames() as $collection) {
      $change_list[$collection] = $config_comparer->getChangelist(NULL, $collection);
    }
    _drush_print_config_changes_table($change_list);
  }
  else {
    $destination_dir = drush_tempdir();
    drush_invoke_process('@self', 'config-export', array(), array('destination' => $destination_dir));
    // @todo Can DiffFormatter produce a CLI pretty diff?
    drush_shell_exec('diff -x %s -u %s %s', '*.git', $destination_dir, $source_dir);
    $output = drush_shell_exec_output();
    drush_print(implode("\n", $output));
  }

  if (drush_confirm(dt('Import the listed configuration changes?'))) {
    if (drush_get_option('partial')) {
      // Partial imports require different processing.
      return drush_op('_drush_config_import_partial', $source_storage);
    }
    return drush_op('_drush_config_import', $config_comparer);
  }
}

// Copied from submitForm() at /core/modules/config/src/Form/ConfigSync.php
function _drush_config_import(StorageComparer $storage_comparer) {
  $config_importer = new ConfigImporter(
    $storage_comparer,
    Drupal::service('event_dispatcher'),
    Drupal::service('config.manager'),
    Drupal::lock(),
    Drupal::service('config.typed'),
    Drupal::moduleHandler(),
    Drupal::service('module_installer'),
    Drupal::service('theme_handler'),
    Drupal::service('string_translation')
  );
  if ($config_importer->alreadyImporting()) {
    drush_log('Another request may be synchronizing configuration already.', 'warning');
  }
  else{
    try {
      $config_importer->import();
      drupal_flush_all_caches();
      drush_log('The configuration was imported successfully.', 'success');
    }
    catch (ConfigException $e) {
      // Return a negative result for UI purposes. We do not differentiate
      // between an actual synchronization error and a failed lock, because
      // concurrent synchronizations are an edge-case happening only when
      // multiple developers or site builders attempt to do it without
      // coordinating.
      watchdog_exception('config_import', $e);
      return drush_set_error('config_import_fail', 'The import failed due to an error. Any errors have been logged.');
    }
  }
}

/**
 * Imports a partial set of configurations.
 */
function _drush_config_import_partial(FileStorage $source) {
  /** @var \Drupal\Core\Config\StorageInterface $active_storage */
  $active_storage = Drupal::service('config.storage');
  foreach ($source->listAll() as $name) {
    $active_storage->write($name, $source->read($name));
  }
}

/**
 * Edit command callback.
 */
function drush_config_edit($config_name = '') {
  if (empty($config_name) && $file = drush_get_option('file', FALSE)) {
    // If not provided, assume config name from the given file.
    $config_name = basename($file, '.yml');
  }

  // Identify and validate input.
  if ($config_name) {
    $config = Drupal::configFactory()->getEditable($config_name);
    if ($config->isNew()) {
      return drush_set_error(dt('Config !name does not exist', array('!name' => $config_name)));
    }
  }
  else {
    $config_names = \Drupal::configFactory()->listAll();
    $choice = drush_choice($config_names, 'Choose a configuration.');
    if (empty($choice)) {
      return drush_user_abort();
    }
    else {
      $config_name = $config_names[$choice];
      $config = Drupal::configFactory()->getEditable($config_name);
    }
  }

  $active_storage = $config->getStorage();
  $contents = $active_storage->read($config_name);

  $temp_storage = new FileStorage(drush_tempdir());
  if ($file) {
    $temp_storage->write($config_name, \Symfony\Component\Yaml\Yaml::parse(file_get_contents($file)));
    // Show difference.
    $existing = new FileStorage(drush_tempdir());
    $existing->write($config_name, $contents);
    // @todo Can DiffFormatter produce a CLI pretty diff?
    drush_shell_exec('diff -u %s %s', $existing->getFilePath($config_name), $temp_storage->getFilePath($config_name));
    $output = drush_shell_exec_output();
    drush_print(implode("\n", $output));

    if (!drush_confirm(dt('Keep these changes?'))) {
      return drush_user_abort(dt('Config not edited.'));
    }
  }
  else {
    // Write tmp YAML file for editing.
    $temp_storage->write($config_name, $contents);

    // $filepath = drush_save_data_to_temp_file();
    $exec = drush_get_editor();
    drush_shell_exec_interactive($exec, $temp_storage->getFilePath($config_name));
  }

  // Perform import operation if user did not immediately exit editor.
  if (!drush_get_option('bg', FALSE)) {
    $new_data = $temp_storage->read($config_name);
    $temp_storage->delete($config_name);
    $config->setData($new_data);
    $config->save();
  }
}

/* Helper functions */

/**
 * Show and return a config object
 *
 * @param $config_name
 *   The config object name.
 */
function drush_config_get_object($config_name) {
  $source = drush_get_option('source', 'active');
  $include_overridden = drush_get_option('include-overridden', FALSE);

  if ($include_overridden) {
    // Displaying overrides only applies to active storage.
    $config = \Drupal::config($config_name);
    $data = $config->get();
  }
  elseif ($source == 'active') {
    $config = \Drupal::service('config.storage');
    $data = $config->read($config_name);
  }
  elseif ($source == 'staging') {
    $config = \Drupal::service('config.storage.staging');
    $data = $config->read($config_name);
  }
  else {
    return drush_set_error(dt('Unknown value !value for config source.', array('!value' => $source)));
  }

  if ($data === FALSE) {
    return drush_set_error(dt('Config !name does not exist in !source configuration.', array('!name' => $config_name, '!source' => $source)));
  }
  if (empty($data)) {
    drush_log(dt('Config !name exists but has no data.', array('!name' => $config_name)), 'notice');
    return;
  }
  return $data;
}

/**
 * Show and return a value from config system.
 *
 * @param $config_name
 *   The config name.
 * @param $key
 *   The config key.
 */
function drush_config_get_value($config_name, $key) {
  $config = Drupal::config($config_name);
  if ($config->isNew()) {
    return drush_set_error(dt('Config !name does not exist', array('!name' => $config_name)));
  }
  $value = $config->get($key);
  $returns[$config_name . ':' . $key] = $value;

  if ($value === NULL) {
    return drush_set_error('DRUSH_CONFIG_ERROR', dt('No matching key found in !name config.', array('!name' => $config_name)));
  }
  else {
    return $returns;
  }
}

/**
 * Print a table of config changes.
 *
 * @param array $config_changes
 *   An array of changes keyed by collection.
 */
function _drush_print_config_changes_table(array $config_changes) {
  if (drush_get_context('DRUSH_NOCOLOR')) {
    $red = "%s";
    $yellow = "%s";
    $green = "%s";
  }
  else {
    $red = "\033[31;40m\033[1m%s\033[0m";
    $yellow = "\033[1;33;40m\033[1m%s\033[0m";
    $green = "\033[1;32;40m\033[1m%s\033[0m";
  }

  $rows = array();
  $rows[] = array('Collection', 'Config', 'Operation');
  foreach ($config_changes as $collection => $changes) {
    foreach ($changes as $change => $configs) {
      switch ($change) {
        case 'delete':
          $colour = $red;
          break;
        case 'update':
          $colour = $yellow;
          break;
        case 'create':
          $colour = $green;
          break;
        default:
          $colour = "%s";
          break;
      }
      foreach($configs as $config) {
        $rows[] = array(
          $collection,
          $config,
          sprintf($colour, $change)
        );
      }
    }
  }
  drush_print_table($rows, TRUE);
}

/**
 * Command argument complete callback.
 */
function config_config_get_complete() {
  return _drush_config_names_complete();
}

/**
 * Command argument complete callback.
 */
function config_config_set_complete() {
  return _drush_config_names_complete();
}

/**
 * Command argument complete callback.
 */
function config_config_view_complete() {
  return _drush_config_names_complete();
}

/**
 * Command argument complete callback.
 */
function config_config_edit_complete() {
  return _drush_config_names_complete();
}

/**
 * Command argument complete callback.
 */
function config_config_import_complete() {
  return _drush_config_directories_complete();
}

/**
 * Command argument complete callback.
 */
function config_config_export_complete() {
  return _drush_config_directories_complete();
}

/**
 * Helper function for command argument complete callback.
 *
 * @return
 *   Array of available config directories.
 */
function _drush_config_directories_complete() {
  drush_bootstrap_max(DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION);
  global $config_directories;
  return array('values' => array_keys($config_directories));
}

/**
 * Helper function for command argument complete callback.
 *
 * @return
 *   Array of available config names.
 */
function _drush_config_names_complete() {
  drush_bootstrap_max();
  return array('values' => $storage = \Drupal::service('config.storage')->listAll());
}
