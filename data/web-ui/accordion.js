/*
  MIT-style license ...
  Author: Mats Lindblad <mats[dot]lindblad[at]gmail[dot]com>
  Inspiration: accordion.js found at script.aculo.us
*/
var Accordion = Class.create();
Accordion.prototype = {
  initialize: function(container, options){
    this.container = $(container);
    this.setOptions(options);
    this.togglers = this.container.select(this.options.toggler);
    this.togglees = this.container.select(this.options.togglee);
    this.togglees.invoke("hide");
    this.togglers.each(function(t, idx){
      t.observe('click', this.show.bindAsEventListener(this, idx));
    }.bind(this));
    this.setCurrent();
  },
  show: function(event, index) {
    var element = Event.element(event);
    if (this.open.toggler == element) return;
    var current = this.open;
    var next = {toggler: element, togglee: this.togglees[index]};
    new Effect.Parallel([new Effect.BlindUp(current.togglee), new Effect.BlindDown(next.togglee)], {duration: 0.5});
    current.toggler.removeClassName(this.options.activeClassName);
    next.toggler.addClassName(this.options.activeClassName);
    this.open = next;
  },
  setOptions: function(options){
    this.options = {
      toggler:          '.toggler', // className of the element you click
      togglee:          '.togglee', // className of the element you toggle
      activeClassName:  'visible', // className to put on the open elements
      defaultTogglee:   null
    };
    Object.extend(this.options, options || {});
  },
  setCurrent: function(){
    if (this.options.defaultTogglee != null) {
      this.togglees.each(function(t, idx){
        if (t.id == this.options.defaultTogglee) {
          t.show();
          this.open = {toggler: this.togglers[idx], togglee: this.togglees[idx]};
          this.togglers[idx].addClassName(this.options.activeClassName);
          return;
        }
      }.bind(this));
    }
    else {
      this.open = {toggler: this.togglers.first(), togglee: this.togglees.first()};
      this.togglers.first().addClassName(this.options.activeClassName);
      this.togglees.first().show();
    }
  }
};