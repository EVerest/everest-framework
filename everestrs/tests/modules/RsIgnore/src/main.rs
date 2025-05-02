//! Integration test for the "ready_received" handling.
//!
//! We assume that every module shall recieve first `on_ready` before forwarding
//! any other call to the user code. The code below recreates the race condition
//! by making calls to the `other` module from within `on_ready` (and adding a
//! delay in `on_ready`).
#![allow(non_snake_case)]
include!(concat!(env!("OUT_DIR"), "/generated.rs"));

use generated::errors::example::{Error as ExampleError, ExampleErrorsError};
use generated::{
    Context, ExampleClientSubscriber, ExampleServiceSubscriber, Module, ModulePublisher,
    OnReadySubscriber,
};
use std::sync::Arc;
use std::{thread, time};

pub struct OneClass {}

impl ExampleServiceSubscriber for OneClass {
    fn uses_something(&self, _context: &Context, _key: String) -> ::everestrs::Result<bool> {
        Ok(true)
    }
}

// The compilation test is that we don't generate the method interfaces for
// the ignored methods.
impl ExampleClientSubscriber for OneClass {}

impl OnReadySubscriber for OneClass {
    fn on_ready(&self, publishers: &ModulePublisher) {
        // Call the other module. This calls should be ignored.
        publishers.example.max_current(12.3).unwrap();
        let error = ExampleError::ExampleErrors(ExampleErrorsError::ExampleErrorA);
        publishers.example.raise_error(error.clone());
        publishers.example.clear_error(error);
    }
}

fn main() {
    let one_class = Arc::new(OneClass {});
    let _module = Module::new(one_class.clone(), one_class.clone(), one_class.clone());
    log::info!("Module initialized");

    loop {
        let dt = time::Duration::from_millis(250);
        thread::sleep(dt);
    }
}
