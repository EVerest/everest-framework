#![allow(non_snake_case)]
include!(concat!(env!("OUT_DIR"), "/generated.rs"));

fn main() {}

#[cfg(test)]
mod tests {
    #[test]
    fn test_duplicate() {
        use crate::generated::errors::errors_duplicate::ExampleErrorsError;
        let _ = ExampleErrorsError::ExampleErrorA;
        let _ = ExampleErrorsError::ExampleErrorB;
        let _ = ExampleErrorsError::ExampleErrorC;
        let _ = ExampleErrorsError::ExampleErrorD;
    }

    #[test]
    fn test_multiple() {
        {
            use crate::generated::errors::errors_multiple::ExampleErrorsError;
            let _ = ExampleErrorsError::ExampleErrorA;
            let _ = ExampleErrorsError::ExampleErrorB;
            let _ = ExampleErrorsError::ExampleErrorC;
            let _ = ExampleErrorsError::ExampleErrorD;
        }

        {
            use crate::generated::errors::errors_multiple::MoreErrorsError;
            let _ = MoreErrorsError::ExampleErrorA;
            let _ = MoreErrorsError::MoreError;
            let _ = MoreErrorsError::SnakeCaseError;
        }
    }

    #[test]
    fn test_selected() {
        use crate::generated::errors::errors_selected::ExampleErrorsError;
        let _ = ExampleErrorsError::ExampleErrorA;
        let _ = ExampleErrorsError::ExampleErrorB;
        // let _ = ExampleErrorsError::ExampleErrorC should not compile.
    }
}
