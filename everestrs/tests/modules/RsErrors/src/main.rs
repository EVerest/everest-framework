#![allow(non_snake_case)]
include!(concat!(env!("OUT_DIR"), "/generated.rs"));

use crate::generated::Context;
use crate::generated::ErrorsMultipleClientSubscriber;
use crate::generated::Module;
use crate::generated::{ModulePublisher, OnReadySubscriber};
use generated::errors::errors_multiple::{Error as ExampleError, ExampleErrorsError};

use std::collections::HashSet;
use std::sync::{atomic::AtomicBool, atomic::Ordering, Arc, Mutex};

struct ErrorCommunacator {
    errors_raised: Mutex<HashSet<ExampleErrorsError>>,
    errors_cleared: Mutex<HashSet<ExampleErrorsError>>,
    finished_on_ready: AtomicBool,
}

impl Eq for ExampleErrorsError {}

impl std::hash::Hash for ExampleErrorsError {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        std::mem::discriminant(self).hash(state);
    }
}

impl OnReadySubscriber for ErrorCommunacator {
    fn on_ready(&self, publishers: &ModulePublisher) {
        let error_a = ExampleError::ExampleErrors(ExampleErrorsError::ExampleErrorA);
        let error_b = ExampleError::ExampleErrors(ExampleErrorsError::ExampleErrorB);
        let error_c = ExampleError::ExampleErrors(ExampleErrorsError::ExampleErrorC);
        publishers.multiple.raise_error(error_a.clone());
        publishers.multiple.raise_error(error_b);
        publishers.multiple.raise_error(error_c);

        publishers.multiple.clear_error(error_a);
        publishers.multiple.clear_all_errors();

        self.finished_on_ready.store(true, Ordering::Relaxed);
    }
}

impl ErrorsMultipleClientSubscriber for ErrorCommunacator {
    fn on_error_raised(&self, _context: &Context, error: ExampleError) {
        let mut raised_set = self.errors_raised.lock().unwrap();
        log::info!("Error raised {:?}", error);
        if let ExampleError::ExampleErrors(inner) = error {
            raised_set.insert(inner.clone());
        }
    }
    fn on_error_cleared(&self, _context: &Context, error: ExampleError) {
        let mut cleared_set = self.errors_cleared.lock().unwrap();
        log::info!("Error cleared {:?}", error);
        if let ExampleError::ExampleErrors(inner) = error {
            cleared_set.insert(inner.clone());
        }
    }
}

impl crate::generated::ErrorsMultipleServiceSubscriber for ErrorCommunacator {}

fn main() {
    let one_class = Arc::new(ErrorCommunacator {
        errors_raised: Mutex::new(HashSet::new()),
        errors_cleared: Mutex::new(HashSet::new()),
        finished_on_ready: AtomicBool::new(false),
    });
    let _module = Module::new(one_class.clone(), one_class.clone(), one_class.clone());

    let mut tests_passed = false;
    loop {
        std::thread::sleep(std::time::Duration::from_millis(250));

        if one_class.finished_on_ready.load(Ordering::Relaxed) & !tests_passed {
            let raised_set = one_class.errors_raised.lock().unwrap();
            let cleared_set = one_class.errors_cleared.lock().unwrap();
            log::info!("Raised Errors: {:?}", raised_set);
            log::info!("Cleared Errors: {:?}", cleared_set);
            assert_eq!(raised_set.len(), 3);
            assert_eq!(cleared_set.len(), 3);
            assert_eq!(*raised_set, *cleared_set);
            tests_passed = true;
        }
    }
}
