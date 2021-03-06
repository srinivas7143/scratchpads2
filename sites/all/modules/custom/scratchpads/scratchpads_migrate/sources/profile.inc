<?php

/**
 * Common mappings for the Drupal 6 node migrations.
 */
class ScratchpadsProfileMigration extends ScratchpadsNodeMigration{

  public $unmigratedDesinations = array();

  public function sourceFieldList(){
    if($this->count_profile_nodes() > 0){
      $fields = parent::sourceFieldList();
    }else{
      $fields = array();
    }
    $fields['location_country'] = t("Country (derived from associated location node)");
    if(scratchpads_migrate_source_table_exists('profile_fields')){
      // Merge in the actual profile fields
      $query = Database::getConnection('default', $this->sourceConnection)->select('profile_fields', 'pf');
      $query->fields('pf', array(
        'name',
        'title'
      ));
      if($this->count_profile_nodes() > 0){
        $query->condition('pf.name', array_keys(DrupalFieldProfileMigration::getFieldNameMappings()), 'NOT IN');
      }
      $fields += $query->execute()->fetchAllKeyed();
    }
    return $fields;
  }

  public function getProfileFields(){
    if(scratchpads_migrate_source_table_exists('profile_fields')){
      $query = Database::getConnection('default', $this->sourceConnection)->select('profile_fields', 'pf');
      $query->fields('pf', array(
        'name'
      ));
      if($this->count_profile_nodes() > 0){
        $query->condition('pf.name', array_keys(DrupalFieldProfileMigration::getFieldNameMappings()), 'NOT IN');
      }
      return $query->execute()->fetchCol();
    }else{
      return array();
    }
  }

  public function __construct(array $arguments){
    parent::__construct($arguments);
    $node_mode = ($this->count_profile_nodes() > 0);
    // Remove all mappings as these will be for nodes, not users
    foreach($this->fieldMappings as $mapping){
      $destination = $mapping->getDestinationField();
      $source = $mapping->getSourceField();
      if(is_numeric($source)){
        $this->addUnmigratedSources(array(
          $source
        ));
      }
      $this->removeFieldMapping($destination);
    }
    if($node_mode){
      $this->map = new MigrateSQLMap($this->machineName, array(
        'nid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'description' => 'Source profile node ID',
          'alias' => 'n'
        )
      ), MigrateDestinationScratchpadsProfile::getKeySchema());
    }else{
      $this->map = new MigrateSQLMap($this->machineName, array(
        'uid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'description' => 'Source UID',
          'alias' => 'u'
        )
      ), MigrateDestinationScratchpadsProfile::getKeySchema());
    }
    $this->destination = new MigrateDestinationScratchpadsProfile(array(
      'md5_passwords' => FALSE
    ));
    $destination_fields = $this->getDestination()->fields();
    if(array_key_exists('field_body', $destination_fields) && $node_mode){
      $this->addFieldMapping('field_body', 'body');
      $this->addUnmigratedDestinations(array(
        'field_body:summary'
      ));
    }
    // Add country (derived from associated location node)
    $this->addFieldMapping('field_user_country', 'location_country');
    // Get profile fields
    $this->profileFieldMappings = array_merge(DrupalFieldContentProfileMigration::getFieldNameMappings(), DrupalFieldProfileMigration::getFieldNameMappings());
    $profile_fields = $this->getProfileFields();
    foreach($this->sourceFieldList() as $key => $field){
      // Is this an image field?
      if($this->sourceFieldTypes[$key] == 'filefield'){
        $this->addFieldMapping('picture', $key)->sourceMigration('Image');
      }else if(array_key_exists($key, $this->profileFieldMappings)){
        // Do we have a mapping for this field?
        $this->addFieldMapping($this->profileFieldMappings[$key], $key);
      }else{
        if(array_key_exists($key, $destination_fields)){
          $this->addFieldMapping($key, $key);
        }elseif(strpos($key, 'profile') !== false || in_array($key, $profile_fields)){ // Profile fields are prefixed with profile
          $this->addFieldMapping(DrupalFieldProfileMigration::destinationFieldName($key), $key);
        }
      }
    }
    if($node_mode){
      $this->addFieldMapping('name', 'title');
      $this->addSimpleMappings(array(
        'path',
        'created'
      ));
      // Node fields we're not using
      $this->addUnmigratedSources(array(
        'menu',
        'language',
        'status',
        'changed',
        'comment',
        'promote',
        'sticky',
        'node_weight'
      ));
    }
    $this->addFieldMapping('uid', 'uid')->defaultValue(0);
    foreach($destination_fields as $key => $field){
      if($node_mode && strpos($key, ':')){
        $subfield = end(explode(':', $key));
        switch($subfield){
          case 'format':
            $this->addFieldMapping($key)->defaultValue('filtered_html');
            break;
          case 'language':
            $this->addFieldMapping($key)->defaultValue('en');
            break;
        }
      }
    }
    $this->addFieldMapping('legal_accept')->defaultValue(1);
    $this->addUnmigratedDestinations(array(
      'group_audience'
    ));
  }

  protected function nodeQuery(){
    if($this->count_profile_nodes() > 0){
      $query = Database::getConnection('default', $this->sourceConnection)->select('node', 'n')->fields('n', array(
        'nid',
        'vid',
        'language',
        'title',
        'uid',
        'status',
        'created',
        'changed',
        'comment',
        'promote',
        'moderate',
        'sticky'
      ))->condition('type', $this->sourceType);
      $query->innerJoin('node_revisions', 'nr', 'n.vid=nr.vid');
      $query->fields('nr', array(
        'body',
        'teaser',
        'format'
      ));
      $query->groupBy('n.title');
      $query->orderBy('nid');
      return $query;
    }else{
      $query = Database::getConnection('default', $this->sourceConnection)->select('profile_values', 'u');
      $query->addExpression('DISTINCT uid', 'uid');
      $query->addExpression('0', 'nid');
      $query->addExpression('0', 'vid');
      $query->addExpression('0', 'sticky');
      return $query;
    }
  }
  // Allow overiding count query
  protected function countQuery(){
    if($this->count_profile_nodes() > 0){
      $query = Database::getConnection('default', $this->sourceConnection)->select('node', 'n');
      $query->addExpression('COUNT(DISTINCT(n.title))');
      $query->innerJoin('node_revisions', 'nr', 'n.vid=nr.vid');
      $query->condition('type', $this->sourceType);
      return $query;
    }else{
      $query = Database::getConnection('default', $this->sourceConnection)->select('profile_values', 'p');
      $query->addExpression('COUNT(DISTINCT p.uid)');
      return $query;
    }
  }

  protected function count_profile_nodes(){
    static $count = NULL;
    if($count !== NULL){return $count;}
    $query = Database::getConnection('default', $this->sourceConnection)->select('node', 'n');
    $query->addExpression('COUNT(DISTINCT(n.title))');
    $query->innerJoin('node_revisions', 'nr', 'n.vid=nr.vid');
    $query->condition('type', $this->sourceType);
    $count = $query->execute()->fetchField();
    return $count;
  }

  /**
   * Implementation of Migration::prepareRow().
   */
  public function prepareRow($row){
    parent::prepareRow($row);
    // Manipulate some of the values
    // Fix Dr. value for some old sites
    if(property_exists($row, 'field_title')){
      if($row->field_title == 'Dr.'){
        $row->field_title = 'Dr';
      }else if(strlen($row->field_title) > 255){
        // Probably spam
        $row->field_title = '';
      }
    }
    // Fix homepage having tags in it?
    if(property_exists($row, 'field_homepage')){
      $row->field_homepage = strip_tags($row->field_homepage);
    }
    if(isset($row->uid)){
      // Get the location node for the content profile - this has the country
      $query = Database::getConnection('default', $this->sourceConnection)->select('location', 'l');
      $query->innerJoin('location_instance', 'li', 'l.lid = li.lid');
      $query->fields('l', array(
        'country'
      ));
      $query->condition('li.uid', $row->uid);
      $query->range(0, 1);
      if($location = $query->execute()->fetchField()){
        $row->location_country = strtoupper($location);
      }
      // Get the core profile fields
      if(scratchpads_migrate_source_table_exists('profile_values')){
        $query = Database::getConnection('default', $this->sourceConnection)->select('profile_values', 'pv');
        $query->addField('pv', 'value');
        $query->innerJoin('profile_fields', 'pf', 'pf.fid = pv.fid');
        $query->addField('pf', 'name');
        $query->condition('pv.uid', $row->uid);
        $profile_fields = $query->execute();
        foreach($profile_fields as $profile_field){
          $row->{$profile_field->name} = $profile_field->value;
        }
      }
    }else{
      // UID gets wiped if it's 0 - add it back in
      $row->uid = 0;
    }
    // Ensure empty fields
    foreach($this->sourceFieldList() as $key => $field){
      if(!property_exists($row, $key)){
        $row->{$key} = NULL;
      }
    }
    // Parse images for row body
    if(isset($row->body)){
      $row->body = ScratchpadsNodeMigration::parseImages($row->body, $row);
    }
  }
}