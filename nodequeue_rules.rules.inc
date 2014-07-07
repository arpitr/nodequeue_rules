<?php

/**
 * @file
 * Rules integration for the nodequeue module.
 */

// FIXES
// persist checkboxes value on rules
// Get Queues Name instead of machine name on cheeckboxes.

/**
  * Implements hook_rules_data_info().
  */
function nodequeue_rules_rules_data_info() {
  $return = array(
    'checkboxes' => array(
      'label' => t('checkboxes'),
      'ui class' => 'NodequeueRulesUIClass',
    )
  );
  return $return;
}

class NodequeueRulesUIClass extends RulesDataUI implements RulesDataDirectInputFormInterface {
  public static function getDefaultMode() {
    return 'input';
  }
  public static function inputForm($name, $info, $settings, RulesPlugin $element) {
    if (!empty($info['options list'])) {
      // Make sure the .rules.inc of the providing module is included as the
      // options list callback may reside there.
      $element->call('loadBasicInclude');
      $form[$name] = array(
        '#type' => 'checkboxes',
        '#options' => call_user_func($info['options list'], $element, $name),
      );
    }
    else {
      $form[$name] = array(
        '#type' => 'textarea',
      );
      RulesDataInputEvaluator::attachForm($form, $settings, $info, $element->availableVariables());
    }
        $settings += array($name => isset($info['default value']) ? $info['default value'] : NULL);
    $form[$name] += array(
      '#title' => t('Value'),
      '#default_value' => $settings[$name],
      '#required' => empty($info['optional']),
      '#after_build' => array('rules_ui_element_fix_empty_after_build'),
      '#rows' => 3,
    );
    return $form;
  }
    public static function render($value) {
    return array(
      'content' => array('#markup' => check_plain($value)),
      '#attributes' => array('class' => array('rules-parameter-text')),
    );
  }
}


/**
 * Implements hook_rules_action_info().
 */
function nodequeue_rules_rules_action_info() {
  return array(
    // Action: Add a given node to the given nodequeue.
    'nodequeue_rules_action_add_node' => array(
      'label' => t('Add node to nodequeue Bottom'),
      'group' => t('Nodequeue'),
      'parameter' => array(
        'node' => array(
          'type' => 'text',
          'label' => t('Node'),
          'description' => t('The node you want to add at the bottom of nodequeue.'),
        ),
        'queue' => array(
          'type' => 'checkboxes',
          'label' => t('Queue'),
          'description' => t('The queue where the node should be added to.'),
          'options list' => 'nodequeue_rules_get_queue_names',
        ),
        'unique' => array(
          'type' => 'boolean',
          'label' => t('Prevent duplication'),
          'description' => t('Selecting this option will prevent the node from being added more than once to any subqueue.'),
          'default value' => TRUE,
        )
      ),
    ),
    'nodequeue_rules_action_add_node_top' => array(
      'label' => t('Add node to nodequeue Top'),
      'group' => t('Nodequeue'),
      'parameter' => array(
        'node' => array(
          'type' => 'text',
          'label' => t('Node'),
          'description' => t('The node you want to add at the top of nodequue.'),
        ),
        'queue' => array(
          'type' => 'checkboxes',
          'label' => t('Queue'),
          'description' => t('The queue where the node should be added to.'),
          'options list' => 'nodequeue_rules_get_queue_names',
        ),
        'unique' => array(
          'type' => 'boolean',
          'label' => t('Prevent duplication'),
          'description' => t('Selecting this option will prevent the node from being added more than once to any subqueue.'),
          'default value' => TRUE,
        )
      ),
    ),
    // Action: Remove the given node from the given nodequeue.
    'nodequeue_rules_action_remove_node' => array(
      'label' => t('Remove node from nodequeue'),
      'group' => t('Nodequeue'),
      'parameter' => array(
        'node' => array(
          'type' => 'text',
          'label' => t('Node ID'),
          'description' => t('The node you want to remove.'),
        ),
        'queue' => array(
          'type' => 'checkboxes',
          'label' => t('Queue'),
          'description' => t('The queue where the node should be removed from.'),
          'options list' => 'nodequeue_rules_get_queue_names',
        ),
      ),
    ),
  );
}

/**
 * Action: Add the given node to the given nodequeue.
 */
function nodequeue_rules_action_add_node($node_id, $queue_name, $unique) {

  $node = node_load($node_id); 
  $queue = nodequeue_load_queues($queue_name);
  $smartqueue_enabled = function_exists('smartqueue_taxonomy_nodequeue_subqueues') ? TRUE : FALSE;
  if ($smartqueue_enabled) {
    $references = smartqueue_taxonomy_nodequeue_subqueues($queue, $node);
  }

  $smartqueue_domain_enabled = function_exists('smartqueue_domain_nodequeue_subqueues') ? TRUE : FALSE;
  if ($smartqueue_domain_enabled) {
    $subqueue_domain = smartqueue_domain_nodequeue_subqueues($queue, $node);
    $node_domains = nodequeue_rules_get_node_domains($node);
  }
  $subqueues = nodequeue_load_subqueues_by_queue($queue_name);
  foreach($subqueues as $subqueue) {
    $add = TRUE;
    // unique means we don't add it if it's in the queue already
    if ($unique) {
      $add = nodequeue_subqueue_position($subqueue->sqid, $node->nid) === FALSE;
    }
    if ($add) {
      if ($smartqueue_domain_enabled) {
        // Do not allow nodes to be added to queues on domains they are not publishted to.
        // The domain id is the last part of the queue reference string.
        // All smartqueue_domain subqueue references also contain ":domain:".
        if (!empty($subqueue->reference) && !empty($node_domains)){
          $reference_array = explode(':', $subqueue->reference);
          $queue_domain_id = end($reference_array);
          //dpm($queue);
          //dpm($subqueue);
          if(strpos($subqueue->reference, ':domain:') && in_array($queue_domain_id, $node_domains)) {
            nodequeue_subqueue_add($queue[$subqueue->name], $subqueue, $node->nid);
          }
        }
      }
      if ($smartqueue_enabled) {
        if (in_array($subqueue->reference, $references)) {
          nodequeue_subqueue_add($queue[$subqueue->name], $subqueue, $node->nid);
        }
      }
      if(!$smartqueue_domain_enabled && !$smartqueue_domain_enabled){
        nodequeue_subqueue_add($queue[$subqueue->name], $subqueue, $node->nid);
      }
    }
  }
}

/**
 * Action: Add the given node to the given nodequeue.
 */
function nodequeue_rules_action_add_node_top($node_id, $queue_name, $unique) {
  $node = node_load($node_id);
  $queue = nodequeue_load_queues($queue_name);
  $smartqueue_enabled = function_exists('smartqueue_taxonomy_nodequeue_subqueues') ? TRUE : FALSE;
  if ($smartqueue_enabled) {
    $references = smartqueue_taxonomy_nodequeue_subqueues($queue, $node);
  }

  $smartqueue_domain_enabled = function_exists('smartqueue_domain_nodequeue_subqueues') ? TRUE : FALSE;
  if ($smartqueue_domain_enabled) {
    $subqueue_domain = smartqueue_domain_nodequeue_subqueues($queue, $node);
    $node_domains = nodequeue_rules_get_node_domains($node);
  }
  $subqueues = nodequeue_load_subqueues_by_queue($queue_name);
  foreach($subqueues as $subqueue) {
    $add = TRUE;
    // unique means we don't add it if it's in the queue already
    if ($unique) {
      $add = nodequeue_subqueue_position($subqueue->sqid, $node->nid) === FALSE;
    }
    if ($add) {
      if ($smartqueue_domain_enabled) {
        // Do not allow nodes to be added to queues on domains they are not publishted to.
        // The domain id is the last part of the queue reference string.
        // All smartqueue_domain subqueue references also contain ":domain:".
        if (!empty($subqueue->reference) && !empty($node_domains)){
          $reference_array = explode(':', $subqueue->reference);
          $queue_domain_id = end($reference_array);

          if(strpos($subqueue->reference, ':domain:') && in_array($queue_domain_id, $node_domains)) {
            nodequeue_subqueue_add_top($queue[$subqueue->name], $subqueue, $node->nid);
          }
        }
      }
      if ($smartqueue_enabled) {
        if (in_array($subqueue->reference, $references)) {
          // Check to use existing
          nodequeue_subqueue_add_top($queue[$subqueue->name], $subqueue, $node->nid);
        }
      }
      if(!$smartqueue_domain_enabled && !$smartqueue_domain_enabled){
        nodequeue_subqueue_add_top($queue[$subqueue->name], $subqueue, $node->nid);
      }
    }
  }
}


/**
 * Action: Remove the given node from the given nodequeue.
 * TO DO: Check if node exists or not.
 */
function nodequeue_rules_action_remove_node($node_id, $queue_name) {
  $node = node_load($node_id);
  $queue = nodequeue_load_queues($queue_name);
  $subqueues = nodequeue_load_subqueues_by_queue($queue_name);
  foreach ($subqueues as $subqueue) {
    if (!$queue[$subqueue->name]->i18n || (isset($node->tnid) && empty($node->tnid))) {
      nodequeue_subqueue_remove_node($subqueue->sqid, $node->nid);
    }
  }
}

/**
 * Return a list of nodequeue names.
 */
function nodequeue_rules_get_queue_names() {
  $items = array();
  $items = _nodequeue_rules_get_all_queues();
  return $items;
}

/**
  * Need doc.
  */
function _nodequeue_rules_get_all_queues() {
  $query = db_select('nodequeue_queue', 'nq')
    ->fields('nq', array('name','title'));
  $queues = $query->execute()->fetchAllKeyed(0, 1);
  return $queues;
}



/**
 * Helper function for fetching possible domain ids for a node.
 */
function nodequeue_rules_get_node_domains($node){
  $node_domains = array();
  if(empty($node->nid) || !function_exists('domain_get_node_domains')){
    return $node_domains;
  }
  $domain_data = domain_get_node_domains($node->nid);
  if(!empty($domain_data['domain_id'])){
    return $domain_data['domain_id'];
  }
  return $node_domains;
}